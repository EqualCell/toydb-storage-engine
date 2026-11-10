#define _POSIX_C_SOURCE 200809L
#include "index.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <stdint.h>
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"benchmark line %d: %s AM=%d PF=%d RM=%d IX=%d\n",__LINE__,#x,AM_Errno,PFerrno,RMerrno,IX_LastError);exit(1);}}while(0)
typedef struct { char stage[40],traffic[12]; double seconds; PF_Stats stats; size_t results; } Metric;
typedef struct { AM_KeyRID pair; char *bytes; size_t length; } Reference;
typedef struct { int code; size_t position; } Code;
static Metric metrics[24];
static int metric_count;
static double started;
static double clock_now(void)
{
    struct timespec t;REQUIRE(!clock_gettime(CLOCK_MONOTONIC,&t));return t.tv_sec+t.tv_nsec/1e9;
}
static void begin(void){PF_ResetStats();started=clock_now();}
static void capture(const char *stage,const char *traffic,size_t results)
{
    Metric *m=&metrics[metric_count++];
    strcpy(m->stage,stage);strcpy(m->traffic,traffic);m->seconds=clock_now()-started;m->stats=PF_GetStats();m->results=results;
}
static void total(const char *stage,int first,int end,double seconds)
{
    Metric *m=&metrics[metric_count++];int i;
    memset(m,0,sizeof(*m));strcpy(m->stage,stage);strcpy(m->traffic,"combined");m->seconds=seconds;
    for(i=first;i<end;i++) {
        m->stats.logical_requests+=metrics[i].stats.logical_requests;m->stats.cache_hits+=metrics[i].stats.cache_hits;
        m->stats.cache_misses+=metrics[i].stats.cache_misses;m->stats.physical_reads+=metrics[i].stats.physical_reads;
        m->stats.physical_writes+=metrics[i].stats.physical_writes;m->stats.page_allocations+=metrics[i].stats.page_allocations;
    }
}
static int line_compare(const void *a,const void *b)
{
    const StudentLine *x=a,*y=b;
    size_t n=x->length<y->length ? x->length:y->length;
    int c=memcmp(x->bytes,y->bytes,n);return c ? c:(x->length>y->length)-(x->length<y->length);
}
static int reference_compare(const void *a,const void *b)
{
    const Reference *x=a,*y=b;
    int c=strncmp(x->pair.key,y->pair.key,255);
    return c ? c:(x->pair.recId>y->pair.recId)-(x->pair.recId<y->pair.recId);
}
static int code_compare(const void *a,const void *b)
{
    const Code *x=a,*y=b;return (x->code>y->code)-(x->code<y->code);
}
static Reference *load_reference(int fd,const StudentData *source,int column,int length)
{
    Reference *ref=calloc(source->count ? source->count:1,sizeof(*ref));
    StudentLine *actual=calloc(source->count ? source->count:1,sizeof(*actual));
    StudentLine *sorted=calloc(source->count ? source->count:1,sizeof(*sorted));
    RM_Scan scan;RM_RID rid;char record[PF_PAGE_SIZE];size_t size,n=0,i;int error;
    REQUIRE(ref && actual && sorted);REQUIRE(!RM_OpenScan(fd,&scan));
    while((error=RM_GetNextRecord(&scan,&rid,record,sizeof(record),&size))==0) {
        REQUIRE(n<source->count);ref[n].bytes=malloc(size);REQUIRE(ref[n].bytes);
        memcpy(ref[n].bytes,record,size);ref[n].length=size;
        REQUIRE(!IX_Key(record,size,column,length,ref[n].pair.key));REQUIRE(!IX_Encode(rid,&ref[n].pair.recId));
        actual[n].bytes=ref[n].bytes;actual[n].length=size;n++;
    }
    REQUIRE(error==RME_EOF && n==source->count);REQUIRE(!RM_CloseScan(&scan));
    if(n)memcpy(sorted,source->lines,n*sizeof(*sorted));
    qsort(actual,n,sizeof(*actual),line_compare);qsort(sorted,n,sizeof(*sorted),line_compare);
    for(i=0;i<n;i++)REQUIRE(!line_compare(actual+i,sorted+i));
    qsort(ref,n,sizeof(*ref),reference_compare);free(actual);free(sorted);return ref;
}
static size_t code_find(const Code *codes,size_t count,int value)
{
    size_t lo=0,hi=count,mid;
    while(lo<hi){mid=(lo+hi)/2;if(codes[mid].code<value)lo=mid+1;else hi=mid;}
    REQUIRE(lo<count && codes[lo].code==value);return codes[lo].position;
}
static int matches(int op,int cmp)
{
    return op==0 || (op==1 && !cmp) || (op==2 && cmp<0) || (op==3 && cmp>0) ||
           (op==4 && cmp<=0) || (op==5 && cmp>=0) || (op==6 && cmp!=0);
}
static size_t query(int index,int records,const Reference *ref,const Code *codes,size_t count,
                    int column,int length,int op,const char *key,int fetch,unsigned char *seen)
{
    int scan,value,error;size_t got=0,expected=0,i,position,size;char record[PF_PAGE_SIZE],actual[255];RM_RID rid;
    memset(seen,0,count);
    if(op==EQUAL) {
        size_t lo=0,hi=count,mid;
        while(lo<hi){mid=(lo+hi)/2;if(strncmp(ref[mid].pair.key,key,length)<0)lo=mid+1;else hi=mid;}
        for(i=lo;i<count && !strncmp(ref[i].pair.key,key,length);i++)expected++;
    } else for(i=0;i<count;i++)if(matches(op,key ? strncmp(ref[i].pair.key,key,length):0))expected++;
    scan=AM_OpenIndexScan(index,'c',length,op ? op:1,(char *)key);REQUIRE(scan>=0);
    while((value=AM_FindNextEntry(scan))>=0) {
        position=code_find(codes,count,value);
        REQUIRE(!seen[position] && matches(op,key ? strncmp(ref[position].pair.key,key,length):0));
        seen[position]=1;got++;
        if(fetch) {
            REQUIRE(!IX_Decode(value,&rid));error=RM_GetRecord(records,rid,record,sizeof(record),&size);REQUIRE(!error);
            REQUIRE(size==ref[position].length && !memcmp(record,ref[position].bytes,size));
            REQUIRE(!IX_Key(record,size,column,length,actual));REQUIRE(!strncmp(actual,ref[position].pair.key,length));
        }
    }
    REQUIRE(value==AME_EOF && got==expected);REQUIRE(!AM_CloseIndexScan(scan));return got;
}
static uint32_t random_next(uint32_t *state){*state=*state*1664525U+1013904223U;return *state;}
int main(int argc,char **argv)
{
    StudentData source;AM_KeyRID *pairs=NULL,*saved=NULL;Reference *ref=NULL;Code *codes=NULL;
    size_t count=0,i,j,distinct=0,*keys=NULL,position,result_count;unsigned char *seen=NULL;
    int records,index,error,method,column,length,frames,policy,skip,height,pages,range_count;
    char key[255];RM_RID rid;RM_Space space;struct stat info;unsigned long long index_bytes,record_bytes;
    double all_start,build_start,sort_time=0,index_seconds;uint32_t seed;
    REQUIRE(argc==11); /* build/query dataset method column length frames policy skip seed ranges */
    method=!strcmp(argv[3],"existing") ? 0:!strcmp(argv[3],"incremental") ? 1:2;
    REQUIRE(method!=2 || !strcmp(argv[3],"bulk"));
    column=atoi(argv[4]);length=atoi(argv[5]);frames=atoi(argv[6]);policy=!strcmp(argv[7],"LRU") ? PF_LRU:PF_MRU;
    skip=atoi(argv[8]);seed=(uint32_t)strtoul(argv[9],NULL,10);range_count=atoi(argv[10]);
    REQUIRE(frames>=3 && column>0 && length>0 && length<=255 && range_count>0);
    REQUIRE(!strcmp(argv[7],"LRU") || !strcmp(argv[7],"MRU"));
    REQUIRE(!Student_Load(argv[2],skip,&source));REQUIRE(source.maximum<=RM_MAX_RECORD);
    for(i=0;i<source.count;i++)REQUIRE(!IX_Key(source.lines[i].bytes,source.lines[i].length,column,length,key));
    PF_Init();REQUIRE(!PF_SetBufferSize(frames));
    if(!strcmp(argv[1],"build")) {
        all_start=clock_now();
        if(method==1) {
            saved=calloc(source.count ? source.count:1,sizeof(*saved));REQUIRE(saved);
            begin();REQUIRE(!RM_CreateFile("students.pf"));records=RM_OpenFile("students.pf",policy);REQUIRE(records>=0);
            REQUIRE(!AM_CreateIndex("students",0,'c',length));index=PF_OpenFileWithPolicy("students.0",policy);REQUIRE(index>=0);
            REQUIRE(!IX_Incremental(records,index,&source,column,length,saved));capture("combined_construct","combined",source.count);
            begin();REQUIRE(!PF_CloseFile(index));REQUIRE(!RM_CloseFile(records));capture("combined_flush","combined",source.count);
        } else {
            begin();REQUIRE(!RM_CreateFile("students.pf"));records=RM_OpenFile("students.pf",policy);REQUIRE(records>=0);
            for(i=0;i<source.count;i++)REQUIRE(!RM_InsertRecord(records,source.lines[i].bytes,source.lines[i].length,&rid));
            REQUIRE(!RM_CloseFile(records));capture("record_create","student",source.count);
            begin();records=RM_OpenFile("students.pf",policy);REQUIRE(records>=0);
            REQUIRE(!IX_Collect(records,column,length,&pairs,&count));REQUIRE(!RM_CloseFile(records));capture("extract","student",count);
            if(method==2){begin();IX_Sort(pairs,count);capture("sort","none",count);sort_time=metrics[metric_count-1].seconds;}
            build_start=clock_now();begin();REQUIRE(!AM_CreateIndex("students",0,'c',length));
            index=PF_OpenFileWithPolicy("students.0",policy);REQUIRE(index>=0);
            error=method==0 ? IX_InsertPairs(index,length,pairs,count):AM_BulkLoad(index,length,pairs,count);REQUIRE(!error);
            capture("index_construct","index",count);
            begin();REQUIRE(!PF_CloseFile(index));capture("index_flush","index",count);
            index_seconds=clock_now()-build_start;
            if(method==2) {
                total("sorted_construct_total",metric_count-2,metric_count,index_seconds);
                strcpy(metrics[metric_count-1].traffic,"index");
            }
            /* Sum the disjoint extraction/sort/construction/flush rows only. */
            j=method==2 ? 5:4;
            total("index_total",1,j,metrics[1].seconds+sort_time+index_seconds);
        }
        j=method==1 ? 2:method==2 ? 5:4;
        total("end_to_end",0,j,clock_now()-all_start);
    } else REQUIRE(!strcmp(argv[1],"query"));
    /* Unmeasured source and structure validation, then close to empty PF cache. */
    records=RM_OpenFile("students.pf",policy);REQUIRE(records>=0);
    ref=load_reference(records,&source,column,length);
    REQUIRE(!RM_GetSpace(records,&space));REQUIRE(!RM_CloseFile(records));
    index=PF_OpenFileWithPolicy("students.0",policy);REQUIRE(index>=0);
    REQUIRE(!AM_ValidateTree(index,length,source.count,&height,&pages));REQUIRE(!PF_CloseFile(index));
    REQUIRE(!stat("students.0",&info));index_bytes=info.st_size;
    REQUIRE(!stat("students.pf",&info));record_bytes=info.st_size;
    codes=calloc(source.count ? source.count:1,sizeof(*codes));keys=calloc(source.count ? source.count:1,sizeof(*keys));seen=calloc(source.count ? source.count:1,1);REQUIRE(codes && keys && seen);
    for(i=0;i<source.count;i++) {
        codes[i].code=ref[i].pair.recId;codes[i].position=i;
        if(!i || strncmp(ref[i-1].pair.key,ref[i].pair.key,length))keys[distinct++]=i;
    }
    qsort(codes,source.count,sizeof(*codes),code_compare);
    if(!strcmp(argv[1],"query")) {
        for(i=distinct;i>1;i--){j=random_next(&seed)%i;position=keys[i-1];keys[i-1]=keys[j];keys[j]=position;}
        for(j=0;j<4;j++) {
            int fetch=j%2,range=j>=2;
            const char *stage=j==0 ? "point_index_cold":j==1 ? "point_fetch_cold":j==2 ? "range_index_cold":"range_fetch_cold";
            begin();index=PF_OpenFileWithPolicy("students.0",policy);REQUIRE(index>=0);
            records=fetch ? RM_OpenFile("students.pf",policy):-1;REQUIRE(!fetch || records>=0);result_count=0;
            if(!range)for(i=0;i<distinct;i++)result_count+=query(index,records,ref,codes,source.count,column,length,1,ref[keys[i]].pair.key,fetch,seen);
            if(range && source.count)for(i=0;i<(size_t)range_count;i++) {
                position=(i*source.count)/(range_count+1);
                result_count+=query(index,records,ref,codes,source.count,column,length,2+i%5,ref[position].pair.key,fetch,seen);
            }
            REQUIRE(!PF_CloseFile(index));if(fetch)REQUIRE(!RM_CloseFile(records));capture(stage,fetch ? "combined":"index",result_count);
        }
    }
    /* Full index scan and below/above-range misses are checked outside timing. */
    index=PF_OpenFileWithPolicy("students.0",policy);records=RM_OpenFile("students.pf",policy);REQUIRE(index>=0 && records>=0);
    query(index,records,ref,codes,source.count,column,length,0,NULL,TRUE,seen);
    memset(key,1,length);query(index,records,ref,codes,source.count,column,length,1,key,TRUE,seen);
    memset(key,127,length);query(index,records,ref,codes,source.count,column,length,1,key,TRUE,seen);
    REQUIRE(!PF_CloseFile(index));REQUIRE(!RM_CloseFile(records));
    puts("method,stage,seconds,logical,hits,misses,physical_reads,physical_writes,results,index_pages,index_bytes,height,record_pages,record_bytes,traffic");
    for(i=0;i<(size_t)metric_count;i++) {
        Metric *m=&metrics[i];
        printf("%s,%s,%.9f,%llu,%llu,%llu,%llu,%llu,%lu,%d,%llu,%d,%llu,%llu,%s\n",argv[3],m->stage,m->seconds,m->stats.logical_requests,m->stats.cache_hits,m->stats.cache_misses,m->stats.physical_reads,m->stats.physical_writes,(unsigned long)m->results,pages,index_bytes,height,space.pages,record_bytes,m->traffic);
    }
    for(i=0;i<source.count;i++)free(ref[i].bytes);
    free(ref);free(codes);free(keys);free(seen);free(pairs);free(saved);Student_Free(&source);return 0;
}
