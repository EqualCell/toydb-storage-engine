#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s RM=%d PF=%d\n",__LINE__,#x,RMerrno,PFerrno); exit(1); } } while(0)
int main(int argc, char **argv)
{
    int fd;
    RM_RID a, b, c, d;
    char out[PF_PAGE_SIZE], large[RM_MAX_RECORD];
    size_t length;
    RM_Scan scan;
    RM_Space space;
    CHECK(argc == 3);
    PF_Init(); CHECK(PF_SetBufferSize(atoi(argv[1])) == PFE_OK);
    CHECK(RM_CreateFile("records.pf") == RME_OK);
    fd = RM_OpenFile("records.pf", atoi(argv[2])); CHECK(fd >= 0);
    CHECK(RM_OpenScan(fd, &scan) == RME_OK);
    CHECK(RM_GetNextRecord(&scan, &a, out, sizeof(out), &length) == RME_EOF);
    CHECK(RM_CloseScan(&scan) == RME_OK);
    CHECK(RM_InsertRecord(fd,"abc",3,&a) == RME_OK);
    CHECK(RM_InsertRecord(fd,"student;longer",14,&b) == RME_OK);
    CHECK(RM_InsertRecord(fd,"z",1,&c) == RME_OK);
    CHECK(a.page==0 && b.page==0 && c.page==0 && c.slot==2);
    CHECK(RM_GetRecord(fd,b,out,2,&length)==RME_TOOSMALL && length==14);
    CHECK(RM_DeleteRecord(fd,b)==RME_OK);
    CHECK(RM_DeleteRecord(fd,b)==RME_NOTFOUND);
    CHECK(RM_GetRecord(fd,c,out,sizeof(out),&length)==RME_OK && length==1 && out[0]=='z');
    CHECK(RM_InsertRecord(fd,"reuse",5,&d)==RME_OK && d.slot==b.slot && d.page==b.page);
    CHECK(RM_InsertRecord(fd,out,RM_MAX_RECORD+1,&d)==RME_TOOLARGE);
    memset(large,'Q',sizeof(large));
    CHECK(RM_InsertRecord(fd,large,sizeof(large),&d)==RME_OK && d.page==1);
    CHECK(RM_CloseFile(fd)==RME_OK);
    fd=RM_OpenFile("records.pf",atoi(argv[2])); CHECK(fd>=0);
    CHECK(RM_GetRecord(fd,d,out,sizeof(out),&length)==RME_OK && length==sizeof(large) && !memcmp(out,large,length));
    CHECK(RM_GetSpace(fd,&space)==RME_OK && space.records==4 && space.pages==2);
    CHECK(space.payload+space.metadata+space.unused==space.pages*PF_PAGE_SIZE);
    CHECK(RM_CloseFile(fd)==RME_OK);
    puts("PASS: slotted insertion/get, eager compaction, RID reuse, exact fit, reopen");
    return 0;
}
