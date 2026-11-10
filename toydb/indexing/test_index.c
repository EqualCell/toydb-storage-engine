#include "index.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s AM=%d PF=%d IX=%d RM=%d\n",__LINE__,#x,AM_Errno,PFerrno,IX_LastError,RMerrno);exit(1);}}while(0)
static void make_data(StudentData *data,int count,int length,int order,int duplicates)
{
    int i,k;
    char digits[20],*record;
    memset(data,0,sizeof(*data));data->count=count;
    data->lines=calloc(count ? count : 1,sizeof(*data->lines));CHECK(data->lines);
    for(i=0;i<count;i++) {
        k=order==1 ? count-1-i : order==2 ? (i*73)%count : i;
        if(duplicates)k/=duplicates;
        record=malloc(length+32);CHECK(record);
        memset(record,' ',length+32);memcpy(record,"id;",3);
        sprintf(digits,"%08d",k);memcpy(record+3,digits,8);
        if(order==3 && length==8)record[3]=(i%3==0 ? 'A' : i%3==1 ? '0' : ' ');
        record[3+length]=';';sprintf(record+4+length,"source=%d",i);
        data->lines[i].bytes=record;data->lines[i].length=4+length+strlen(record+4+length);
    }
}
static void populate(int fd,const StudentData *data,int length,AM_KeyRID *reference)
{
    size_t i;
    RM_RID rid;
    for(i=0;i<data->count;i++) {
        CHECK(!RM_InsertRecord(fd,data->lines[i].bytes,data->lines[i].length,&rid));
        CHECK(!IX_Key(data->lines[i].bytes,data->lines[i].length,2,length,reference[i].key));
        CHECK(!IX_Encode(rid,&reference[i].recId));
    }
}
static int predicate(int op,int cmp)
{
    return op==0 || (op==1 && cmp==0) || (op==2 && cmp<0) || (op==3 && cmp>0) ||
           (op==4 && cmp<=0) || (op==5 && cmp>=0) || (op==6 && cmp!=0);
}
static void verify_scan(int records,int index,const StudentData *data,const AM_KeyRID *ref,
                        int length,int op,const char *key)
{
    int scan,result,i,found,count=0,expected=0;
    unsigned char *seen=calloc(data->count ? data->count : 1,1);
    char record[PF_PAGE_SIZE],actual[255];
    size_t bytes;
    RM_RID rid;
    CHECK(seen);
    for(i=0;i<(int)data->count;i++)if(predicate(op,key ? strncmp(ref[i].key,key,length) : 0))expected++;
    scan=AM_OpenIndexScan(index,'c',length,op ? op : 1,(char *)key);CHECK(scan>=0);
    while((result=AM_FindNextEntry(scan))>=0) {
        found=-1;
        for(i=0;i<(int)data->count;i++)if(ref[i].recId==result){found=i;break;}
        CHECK(found>=0 && !seen[found] && predicate(op,key ? strncmp(ref[found].key,key,length) : 0));
        seen[found]=1;count++;
        CHECK(!IX_Decode(result,&rid));CHECK(!RM_GetRecord(records,rid,record,sizeof(record),&bytes));
        CHECK(bytes==data->lines[found].length && !memcmp(record,data->lines[found].bytes,bytes));
        CHECK(!IX_Key(record,bytes,2,length,actual));CHECK(!strncmp(actual,ref[found].key,length));
    }
    CHECK(result==AME_EOF && count==expected);
    CHECK(!AM_CloseIndexScan(scan));free(seen);
}
static void conversion(void)
{
    RM_RID rid,back;
    int code;
    char key[255];
    CHECK((PF_PAGE_SIZE-RM_HEADER_SIZE)/RM_SLOT_SIZE<1024);
    rid.page=INT_MAX/1024;rid.slot=1023;
    CHECK(!IX_Encode(rid,&code) && code==INT_MAX && !IX_Decode(code,&back) && back.page==rid.page && back.slot==rid.slot);
    rid.page++;CHECK(IX_Encode(rid,&code)==IX_INVALID);
    rid.page=-1;CHECK(IX_Encode(rid,&code)==IX_INVALID);
    rid.page=0;rid.slot=1024;CHECK(IX_Encode(rid,&code)==IX_INVALID);
    CHECK(IX_Decode(-7,&rid)==IX_INVALID);
    CHECK(!IX_Key("a;00 A 001;z",12,2,8,key) && !memcmp(key,"00 A 001",8));
    CHECK(IX_Key("a;short",7,2,8,key)==IX_INVALID);
    CHECK(IX_Key("a;b",3,3,1,key)==IX_INVALID);
    CHECK(IX_Key("a;x\0x",5,2,3,key)==IX_INVALID);
}
int main(int argc,char **argv)
{
    StudentData data;
    AM_KeyRID *ref,*pairs;
    RM_RID rid;
    char record[PF_PAGE_SIZE],key[255];
    size_t count,bytes;
    int method,n,length,order,dup,records,index,i,height,pages,mode;
    CHECK(argc==10);
    method=atoi(argv[1]);n=atoi(argv[2]);length=atoi(argv[3]);order=atoi(argv[4]);dup=atoi(argv[5]);mode=atoi(argv[8]);
    PF_Init();CHECK(!PF_SetBufferSize(atoi(argv[6])));conversion();
    make_data(&data,n,length,order,dup);
    ref=calloc(n+1,sizeof(*ref));CHECK(ref);
    CHECK(!RM_CreateFile("students.pf"));records=RM_OpenFile("students.pf",atoi(argv[7]));CHECK(records>=0);
    CHECK(!AM_CreateIndex("students",0,'c',length));
    index=PF_OpenFileWithPolicy("students.0",atoi(argv[7]));CHECK(index>=0);
    if(method==1)CHECK(!IX_Incremental(records,index,&data,2,length,ref));
    else {
        populate(records,&data,length,ref);
        if(method==0)CHECK(!IX_BuildExisting(records,index,2,length));
        else {
            CHECK(!IX_Collect(records,2,length,&pairs,&count));IX_Sort(pairs,count);
            CHECK(!AM_BulkLoad(index,length,pairs,count));free(pairs);
        }
    }
    CHECK(!AM_ValidateTree(index,length,n,&height,&pages));
    if(n>100 && length==250)CHECK(height>=3);
    verify_scan(records,index,&data,ref,length,0,NULL);
    for(i=0;i<n;i++)if(i<5 || i%17==0)verify_scan(records,index,&data,ref,length,1,ref[i].key);
    if(n)for(i=1;i<=6;i++)verify_scan(records,index,&data,ref,length,i,ref[n/2].key);
    memset(key,'~',length);verify_scan(records,index,&data,ref,length,1,key);
    memset(key,1,length);verify_scan(records,index,&data,ref,length,1,key);
    CHECK(!PF_CloseFile(index));CHECK(!RM_CloseFile(records));
    records=RM_OpenFile("students.pf",atoi(argv[7]));index=PF_OpenFileWithPolicy("students.0",atoi(argv[7]));CHECK(records>=0 && index>=0);
    verify_scan(records,index,&data,ref,length,0,NULL);
    if(n)for(i=0;i<3000;i++) {
        int s=AM_OpenIndexScan(index,'c',length,1,ref[i%n].key),v;
        CHECK(s>=0);while((v=AM_FindNextEntry(s))>=0){}CHECK(v==AME_EOF);CHECK(!AM_CloseIndexScan(s));
        if(i%101==0){s=AM_OpenIndexScan(index,'c',length,1,key);CHECK(s>=0 && AM_FindNextEntry(s)==AME_EOF);CHECK(!AM_CloseIndexScan(s));}
    }
    /* Ordinary insertion into the bulk-built tree, including capacity boundaries. */
    if(mode && method==2) {
        snprintf(record,sizeof(record),"id;%0*d;after-bulk",length,99999999);
        bytes=strlen(record);
        CHECK(!RM_InsertRecord(records,record,bytes,&rid));CHECK(!IX_Encode(rid,&i));CHECK(!IX_Key(record,bytes,2,length,key));
        CHECK(!AM_InsertEntry(index,'c',length,key,i));
        CHECK(!AM_ValidateTree(index,length,n+1,&height,&pages));
        i=AM_OpenIndexScan(index,'c',length,1,key);CHECK(i>=0 && AM_FindNextEntry(i)>=0 && AM_FindNextEntry(i)==AME_EOF);CHECK(!AM_CloseIndexScan(i));
    }
    CHECK(!PF_CloseFile(index));CHECK(!RM_CloseFile(records));
    for(i=0;i<AM_MAXSTACK;i++)CHECK(!AM_PushStack(i,0));CHECK(AM_PushStack(0,0)==AME_STACK);CHECK(!AM_EmptyStack());CHECK(AM_PopStack()==AME_STACK);
    Student_Free(&data);free(ref);
    printf("PASS method=%d records=%d length=%d order=%d dup=%d frames=%s policy=%s height=%d pages=%d\n",method,n,length,order,dup,argv[6],argv[7],height,pages);
    return 0;
}
