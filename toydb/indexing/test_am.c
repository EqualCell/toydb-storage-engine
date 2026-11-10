#include "index.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AM regression line %d: %s PF=%d\n",__LINE__,#x,PFerrno);exit(1);}}while(0)
int main(int argc,char **argv)
{
    int frames,policy,a,b,i,j,s,v,height,pages,number;
    char key[250],*page;
    AM_KeyRID pairs[165];
    CHECK(argc==3);frames=atoi(argv[1]);policy=atoi(argv[2]);PF_Init();CHECK(!PF_SetBufferSize(frames));
    CHECK(!AM_CreateIndex("first",0,'c',8));a=PF_OpenFileWithPolicy("first.0",policy);CHECK(a>=0);
    if(frames<3) {
        for(i=0;i<62;i++){snprintf(key,sizeof(key),"%08d",i);CHECK(!AM_InsertEntry(a,'c',8,key,i));}
        snprintf(key,sizeof(key),"%08d",62);CHECK(AM_InsertEntry(a,'c',8,key,62)==AME_PF);
        CHECK(!PF_CloseFile(a));a=PF_OpenFile("first.0");CHECK(a>=0);
        CHECK(!AM_ValidateTree(a,8,62,&height,&pages));CHECK(!PF_CloseFile(a));
        puts("PASS: insufficient split buffers release pins and preserve old root");return 0;
    }
    CHECK(!AM_CreateIndex("second",0,'c',250));b=PF_OpenFileWithPolicy("second.0",policy);CHECK(b>=0);
    for(i=0;i<500;i++) {
        memset(key,' ',250);snprintf(key,9,"%08d",i);
        CHECK(!AM_InsertEntry(a,'c',8,key,i));CHECK(!AM_InsertEntry(b,'c',250,key,i));
        for(j=0;j<6;j++) {
            s=AM_OpenIndexScan(j%2 ? a:b,'c',j%2 ? 8:250,EQUAL,key);CHECK(s>=0);
            CHECK(AM_FindNextEntry(s)==i && AM_FindNextEntry(s)==AME_EOF);CHECK(!AM_CloseIndexScan(s));
        }
        memset(key,'~',250);s=AM_OpenIndexScan(b,'c',250,EQUAL,key);CHECK(s>=0 && AM_FindNextEntry(s)==AME_EOF);CHECK(!AM_CloseIndexScan(s));
    }
    CHECK(!AM_ValidateTree(a,8,500,&height,&pages));CHECK(!AM_ValidateTree(b,250,500,&height,&pages) && height>=3);
    CHECK(AM_DeleteEntry(a,'c',8,key,999999)==AME_NOTFOUND); /* missing deletion must release its search pin */
    CHECK(!PF_CloseFile(a));CHECK(!PF_CloseFile(b));
    a=PF_OpenFile("first.0");b=PF_OpenFile("second.0");CHECK(a>=0 && b>=0);
    CHECK(!AM_ValidateTree(a,8,500,&height,&pages));CHECK(!AM_ValidateTree(b,250,500,&height,&pages));
    CHECK(!PF_CloseFile(a));CHECK(!PF_CloseFile(b));
    CHECK(!AM_CreateIndex("bulk-limit",0,'c',8));a=PF_OpenFile("bulk-limit.0");CHECK(a>=0);
    for(i=0;i<165;i++){memset(pairs[i].key,0,255);memcpy(pairs[i].key,"00000001",8);pairs[i].recId=i;}
    CHECK(AM_BulkLoad(a,8,pairs,165)==AME_DUPLIMIT);CHECK(!AM_ValidateTree(a,8,0,&height,&pages));
    CHECK(!AM_BulkLoad(a,8,pairs,164));CHECK(!AM_ValidateTree(a,8,164,&height,&pages));
    CHECK(AM_InsertEntry(a,'c',8,pairs[0].key,165)==AME_DUPLIMIT);
    CHECK(AM_InsertEntry(a,'c',8,"00000002",166)==AME_DUPLIMIT);
    CHECK(!AM_ValidateTree(a,8,164,&height,&pages));CHECK(!PF_CloseFile(a));
    CHECK(!AM_CreateIndex("bad",0,'c',8));a=PF_OpenFile("bad.0");CHECK(a>=0);
    CHECK(!PF_GetFirstPage(a,&number,&page));CHECK(AM_BulkLoad(a,8,pairs,1)==AME_PF);CHECK(!PF_UnfixPage(a,number,FALSE));CHECK(!PF_CloseFile(a));
    puts("PASS: interleaved files, hits/misses/inserts, deep splits, duplicate bound, failed pin paths");return 0;
}
