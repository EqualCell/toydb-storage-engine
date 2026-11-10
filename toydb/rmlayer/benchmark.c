#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "fixed.h"
#include "student.h"
#include "../pflayer/pftypes.h"
#define REQUIRE(x) do { if(!(x)) { fprintf(stderr,"benchmark line %d: %s RM=%d PF=%d\n",__LINE__,#x,RMerrno,PFerrno); exit(1); } } while(0)

struct stored { RM_RID rid; size_t input; };
static int compare_rid(const void *left,const void *right)
{
    const struct stored *a=left,*b=right;
    if(a->rid.page!=b->rid.page) return a->rid.page<b->rid.page ? -1 : 1;
    return (a->rid.slot>b->rid.slot)-(a->rid.slot<b->rid.slot);
}
static double now(void)
{
    struct timespec time;
    REQUIRE(clock_gettime(CLOCK_MONOTONIC,&time)==0);
    return time.tv_sec + time.tv_nsec/1000000000.0;
}
/* Only dispatch for the comparison driver; each format has its own implementation. */
static int open_storage(char *path,int fixed,int policy,unsigned maximum,FM_File *file)
{
    if(fixed) return FM_OpenFile(path,policy,maximum,file);
    file->fd=RM_OpenFile(path,policy);
    return file->fd<0 ? file->fd : RME_OK;
}
static int close_storage(int fixed,FM_File *file)
{
    return fixed ? FM_CloseFile(file) : RM_CloseFile(file->fd);
}
static int open_scan(int fixed,FM_File *file,FM_Scan *scan)
{
    return fixed ? FM_OpenScan(file,scan) : RM_OpenScan(file->fd,&scan->cursor);
}
static int next_record(int fixed,FM_Scan *scan,RM_RID *rid,char *out,size_t *length)
{
    return fixed ? FM_GetNextRecord(scan,rid,out,PF_PAGE_SIZE,length) :
                   RM_GetNextRecord(&scan->cursor,rid,out,PF_PAGE_SIZE,length);
}
static int close_scan(int fixed,FM_Scan *scan)
{
    return fixed ? FM_CloseScan(scan) : RM_CloseScan(&scan->cursor);
}
int main(int argc,char **argv)
{
    StudentData data;
    struct stored *stored;
    struct stat info;
    FM_File file;
    FM_Scan scan;
    RM_RID rid;
    RM_Space space;
    PF_Stats insert_stats,scan_stats;
    int fixed,policy,error;
    unsigned maximum;
    size_t i,length,count=0;
    unsigned long long scanned_payload=0,pf_overhead;
    char out[PF_PAGE_SIZE];
    double start,insert_seconds,scan_seconds,utilization;
    if(argc==2 && !strcmp(argv[1],"--limits")) {
        printf("%d %d %d\n",PF_PAGE_SIZE,RM_MAX_RECORD,FM_MAX_RECORD);
        return 0;
    }
    REQUIRE(argc==8);
    REQUIRE(!strcmp(argv[3],"slotted") || !strcmp(argv[3],"fixed"));
    fixed=!strcmp(argv[3],"fixed"); maximum=(unsigned)strtoul(argv[4],NULL,10);
    REQUIRE(!strcmp(argv[6],"LRU") || !strcmp(argv[6],"MRU"));
    policy=!strcmp(argv[6],"LRU") ? PF_LRU : PF_MRU;
    REQUIRE(Student_Load(argv[1],atoi(argv[7]),&data)==0);
    /* Preflight the whole import before creating a database. No truncation. */
    REQUIRE(data.maximum <= (fixed ? maximum : RM_MAX_RECORD));
    REQUIRE(!fixed || (maximum>0 && maximum<=FM_MAX_RECORD));
    stored=calloc(data.count ? data.count : 1,sizeof(*stored)); REQUIRE(stored!=NULL);
    PF_Init(); REQUIRE(PF_SetBufferSize(atoi(argv[5]))==PFE_OK);
    REQUIRE(RM_CreateFile(argv[2])==RME_OK);
    REQUIRE(open_storage(argv[2],fixed,policy,maximum,&file)==RME_OK);
    PF_ResetStats(); start=now();
    for(i=0;i<data.count;i++) {
        error=fixed ? FM_InsertRecord(&file,data.lines[i].bytes,data.lines[i].length,&stored[i].rid) :
                      RM_InsertRecord(file.fd,data.lines[i].bytes,data.lines[i].length,&stored[i].rid);
        REQUIRE(error==RME_OK); stored[i].input=i;
    }
    REQUIRE(close_storage(fixed,&file)==RME_OK);
    insert_seconds=now()-start; insert_stats=PF_GetStats();

    /* Include reopen and cursor setup: their page checks belong to the scan. */
    PF_ResetStats(); start=now();
    REQUIRE(open_storage(argv[2],fixed,policy,maximum,&file)==RME_OK);
    REQUIRE(open_scan(fixed,&file,&scan)==RME_OK);
    while((error=next_record(fixed,&scan,&rid,out,&length))==RME_OK) {
        count++; scanned_payload+=length;
    }
    REQUIRE(error==RME_EOF);
    REQUIRE(close_scan(fixed,&scan)==RME_OK);
    REQUIRE(close_storage(fixed,&file)==RME_OK);
    scan_seconds=now()-start; scan_stats=PF_GetStats();
    REQUIRE(count==data.count && scanned_payload==data.payload);

    /* Validation and byte accounting are outside both measured intervals. */
    REQUIRE(open_storage(argv[2],fixed,policy,maximum,&file)==RME_OK);
    for(i=0;i<data.count;i++) {
        error=fixed ? FM_GetRecord(&file,stored[i].rid,out,sizeof(out),&length) :
                      RM_GetRecord(file.fd,stored[i].rid,out,sizeof(out),&length);
        REQUIRE(error==RME_OK && length==data.lines[i].length && !memcmp(out,data.lines[i].bytes,length));
    }
    qsort(stored,data.count,sizeof(*stored),compare_rid);
    REQUIRE(open_scan(fixed,&file,&scan)==RME_OK);
    i=0;
    while((error=next_record(fixed,&scan,&rid,out,&length))==RME_OK) {
        REQUIRE(i<data.count && rid.page==stored[i].rid.page && rid.slot==stored[i].rid.slot);
        REQUIRE(length==data.lines[stored[i].input].length && !memcmp(out,data.lines[stored[i].input].bytes,length));
        i++;
    }
    REQUIRE(error==RME_EOF && i==data.count);
    REQUIRE(close_scan(fixed,&scan)==RME_OK);
    REQUIRE((fixed ? FM_GetSpace(&file,&space) : RM_GetSpace(file.fd,&space))==RME_OK);
    REQUIRE(close_storage(fixed,&file)==RME_OK);
    REQUIRE(stat(argv[2],&info)==0);
    REQUIRE(space.records==data.count && space.payload==data.payload);
    REQUIRE(space.payload+space.metadata+space.padding+space.unused==space.pages*PF_PAGE_SIZE);
    pf_overhead=sizeof(PFhdr_str)+space.pages*(sizeof(PFfpage)-PF_PAGE_SIZE);
    REQUIRE((unsigned long long)info.st_size==space.pages*PF_PAGE_SIZE+pf_overhead);
    utilization=space.pages ? 100.0*space.payload/(space.pages*PF_PAGE_SIZE) : 0;
    puts("format,max_length,buffer_capacity,policy,record_count,payload_bytes,data_pages,metadata_pages,file_bytes,record_metadata_bytes,padding_bytes,unused_bytes,pf_overhead_bytes,utilization_percent,insert_seconds,scan_seconds,insert_logical,insert_hits,insert_misses,insert_reads,insert_writes,scan_logical,scan_hits,scan_misses,scan_reads,scan_writes");
    printf("%s,%u,%s,%s,%llu,%llu,%llu,0,%llu,%llu,%llu,%llu,%llu,%.6f,%.9f,%.9f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
           argv[3],fixed ? maximum : 0,argv[5],argv[6],space.records,space.payload,space.pages,
           (unsigned long long)info.st_size,space.metadata,space.padding,space.unused,pf_overhead,
           utilization,insert_seconds,scan_seconds,insert_stats.logical_requests,insert_stats.cache_hits,
           insert_stats.cache_misses,insert_stats.physical_reads,insert_stats.physical_writes,
           scan_stats.logical_requests,scan_stats.cache_hits,scan_stats.cache_misses,
           scan_stats.physical_reads,scan_stats.physical_writes);
    free(stored); Student_Free(&data);
    return 0;
}
