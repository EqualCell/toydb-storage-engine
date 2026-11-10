#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "fixed.h"
/* Only this executable wraps system calls; no production fault hooks. */
static int fail_read,fail_write;
extern ssize_t __real_read(int,void *,size_t);
extern ssize_t __real_write(int,const void *,size_t);
ssize_t __wrap_read(int fd,void *out,size_t size)
{
    if(fail_read && size>PF_PAGE_SIZE) { errno=EIO; return -1; }
    return __real_read(fd,out,size);
}
ssize_t __wrap_write(int fd,const void *in,size_t size)
{
    if(fail_write && size>PF_PAGE_SIZE) { errno=EIO; return -1; }
    return __real_write(fd,in,size);
}
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"failure test line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(void)
{
    int fd;
    RM_RID a,b;
    char large[RM_MAX_RECORD],out[PF_PAGE_SIZE];
    size_t length;
    RM_Scan scan;
    FM_File fixed;
    FM_Scan fixed_scan;
    memset(large,'a',sizeof(large));
    PF_Init(); CHECK(PF_SetBufferSize(1)==PFE_OK);
    CHECK(RM_CreateFile("failure.pf")==RME_OK);
    fd=RM_OpenFile("failure.pf",PF_LRU); CHECK(fd>=0);
    CHECK(RM_InsertRecord(fd,large,sizeof(large),&a)==RME_OK);
    fail_write=1;
    CHECK(RM_InsertRecord(fd,"b",1,&b)==RME_PF && PFerrno==PFE_UNIX);
    CHECK(RM_CloseFile(fd)==RME_PF && PFerrno==PFE_UNIX);
    fail_write=0;
    CHECK(RM_GetRecord(fd,a,out,sizeof(out),&length)==RME_OK && !memcmp(out,large,length));
    CHECK(RM_InsertRecord(fd,"b",1,&b)==RME_OK);
    CHECK(RM_CloseFile(fd)==RME_OK);
    fail_read=1;
    CHECK(RM_OpenFile("failure.pf",PF_LRU)==RME_PF && PFerrno==PFE_UNIX);
    fail_read=0;
    fd=RM_OpenFile("failure.pf",PF_LRU); CHECK(fd>=0);
    CHECK(RM_OpenScan(fd,&scan)==RME_OK);
    CHECK(RM_GetNextRecord(&scan,&a,out,sizeof(out),&length)==RME_OK);
    fail_read=1;
    CHECK(RM_GetNextRecord(&scan,&a,out,sizeof(out),&length)==RME_PF && PFerrno==PFE_UNIX);
    fail_read=0;
    CHECK(RM_GetNextRecord(&scan,&a,out,sizeof(out),&length)==RME_OK && a.page==b.page && a.slot==b.slot && length==1 && out[0]=='b');
    CHECK(RM_GetNextRecord(&scan,&a,out,sizeof(out),&length)==RME_EOF);
    CHECK(RM_GetRecord(fd,b,out,sizeof(out),&length)==RME_OK && length==1 && out[0]=='b');
    CHECK(RM_CloseScan(&scan)==RME_OK);
    CHECK(RM_CloseFile(fd)==RME_OK);
    CHECK(FM_CreateFile("fixed-failure.pf")==RME_OK);
    CHECK(FM_OpenFile("fixed-failure.pf",PF_MRU,4000,&fixed)==RME_OK);
    CHECK(FM_InsertRecord(&fixed,large,4000,&a)==RME_OK);
    CHECK(FM_InsertRecord(&fixed,"b",1,&b)==RME_OK);
    CHECK(FM_CloseFile(&fixed)==RME_OK);
    CHECK(FM_OpenFile("fixed-failure.pf",PF_MRU,4000,&fixed)==RME_OK);
    CHECK(FM_OpenScan(&fixed,&fixed_scan)==RME_OK);
    CHECK(FM_GetNextRecord(&fixed_scan,&a,out,sizeof(out),&length)==RME_OK);
    fail_read=1;
    CHECK(FM_GetNextRecord(&fixed_scan,&a,out,sizeof(out),&length)==RME_PF);
    fail_read=0;
    CHECK(FM_GetNextRecord(&fixed_scan,&a,out,sizeof(out),&length)==RME_OK && a.page==b.page && a.slot==b.slot && length==1 && out[0]=='b');
    CHECK(FM_GetNextRecord(&fixed_scan,&a,out,sizeof(out),&length)==RME_EOF);
    CHECK(FM_CloseScan(&fixed_scan)==RME_OK);
    CHECK(FM_CloseFile(&fixed)==RME_OK);
    puts("PASS: PF read/write failures propagated; pins released; data survives retry");
    return 0;
}
