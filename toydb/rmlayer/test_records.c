#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fixed.h"
#include "internal.h"
#define SEED 20261104U
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"seed=%u line %d: %s RM=%d PF=%d\n",SEED,__LINE__,#x,RMerrno,PFerrno); exit(1); } } while(0)

static void boundaries(int policy)
{
    int fd,i;
    RM_RID a,b,c,d,bad;
    RM_Scan scan;
    RM_Space before,after;
    char out[PF_PAGE_SIZE], data[RM_MAX_RECORD];
    size_t length;
    memset(data,0x5a,sizeof(data));
    CHECK(RM_CreateFile("bounds.pf")==RME_OK);
    fd=RM_OpenFile("bounds.pf",policy); CHECK(fd>=0);
    CHECK(RM_InsertRecord(fd,data,0,&a)==RME_INVALID);
    CHECK(RM_InsertRecord(fd,data,sizeof(data),&a)==RME_OK && a.page==0 && a.slot==0);
    CHECK(RM_GetSpace(fd,&before)==RME_OK && before.unused==0);
    CHECK(RM_InsertRecord(fd,data,sizeof(data)+1,&b)==RME_TOOLARGE);
    CHECK(RM_GetSpace(fd,&after)==RME_OK && !memcmp(&before,&after,sizeof(before)));
    CHECK(RM_GetRecord(fd,a,NULL,0,&length)==RME_TOOSMALL && length==sizeof(data));
    memset(out,0x33,sizeof(out));
    CHECK(RM_GetRecord(fd,a,out,3,&length)==RME_TOOSMALL && out[0]==0x33 && out[3]==0x33);
    bad=a; bad.page=99; CHECK(RM_GetRecord(fd,bad,out,sizeof(out),&length)==RME_NOTFOUND);
    bad=a; bad.slot=99; CHECK(RM_DeleteRecord(fd,bad)==RME_NOTFOUND);
    bad=a; bad.slot=-1; CHECK(RM_GetRecord(fd,bad,out,sizeof(out),&length)==RME_NOTFOUND);
    CHECK(RM_GetRecord(-1,a,out,sizeof(out),&length)==RME_PF && PFerrno==PFE_FD);
    CHECK(RM_InsertRecord(fd,"a",1,&b)==RME_OK && b.page==1);
    CHECK(RM_InsertRecord(fd,"middle",6,&c)==RME_OK);
    CHECK(RM_InsertRecord(fd,"last",4,&d)==RME_OK);
    CHECK(RM_DeleteRecord(fd,c)==RME_OK);
    CHECK(RM_GetRecord(fd,d,out,sizeof(out),&length)==RME_OK && length==4 && !memcmp(out,"last",4));
    CHECK(RM_DeleteRecord(fd,b)==RME_OK);
    CHECK(RM_DeleteRecord(fd,d)==RME_OK);
    CHECK(RM_DeleteRecord(fd,d)==RME_NOTFOUND);
    CHECK(RM_DeleteRecord(fd,a)==RME_OK);
    CHECK(RM_InsertRecord(fd,data,sizeof(data)-1,&a)==RME_OK && a.page==0 && a.slot==0);
    CHECK(RM_GetSpace(fd,&after)==RME_OK && after.pages==2);
    CHECK(RM_OpenScan(fd,&scan)==RME_OK);
    CHECK(RM_GetNextRecord(&scan,&b,out,1,&length)==RME_TOOSMALL);
    CHECK(RM_GetNextRecord(&scan,&b,out,sizeof(out),&length)==RME_OK && b.page==a.page && b.slot==a.slot);
    CHECK(RM_GetNextRecord(&scan,&b,out,sizeof(out),&length)==RME_EOF);
    CHECK(RM_GetNextRecord(&scan,&b,out,sizeof(out),&length)==RME_EOF);
    CHECK(RM_CloseScan(&scan)==RME_OK);
    CHECK(RM_GetNextRecord(&scan,&b,out,sizeof(out),&length)==RME_SCAN);
    /* Empty page 0 precedes a live record on page 1. */
    CHECK(RM_InsertRecord(fd,"live",4,&b)==RME_OK && b.page==1);
    CHECK(RM_DeleteRecord(fd,a)==RME_OK);
    for(i=0;i<100;i++) {
        CHECK(RM_InsertRecord(fd,data,1000,&a)==RME_OK && a.page==0);
        CHECK(RM_DeleteRecord(fd,a)==RME_OK);
    }
    CHECK(RM_CloseFile(fd)==RME_OK);
    fd=RM_OpenFile("bounds.pf",policy); CHECK(fd>=0);
    CHECK(RM_OpenScan(fd,&scan)==RME_OK);
    CHECK(RM_GetNextRecord(&scan,&c,out,sizeof(out),&length)==RME_OK && c.page==b.page && c.slot==b.slot && length==4 && !memcmp(out,"live",4));
    CHECK(RM_GetNextRecord(&scan,&c,out,sizeof(out),&length)==RME_EOF);
    CHECK(RM_CloseScan(&scan)==RME_OK);
    CHECK(RM_CloseFile(fd)==RME_OK); /* A leaked pin makes close fail. */
}

#define REFERENCES 160
struct reference { int live; RM_RID rid; size_t length; unsigned char data[750]; };
static struct reference reference[REFERENCES];
static uint32_t random_state=SEED;
static unsigned next_random(void)
{
    random_state=random_state*1664525U+1013904223U;
    return random_state;
}
static void check_scan(int fd)
{
    RM_Scan scan;
    RM_RID rid;
    int seen[REFERENCES]={0},error,i,found;
    unsigned char data[PF_PAGE_SIZE];
    size_t length;
    CHECK(RM_OpenScan(fd,&scan)==RME_OK);
    while((error=RM_GetNextRecord(&scan,&rid,data,sizeof(data),&length))==RME_OK) {
        found=-1;
        for(i=0;i<REFERENCES;i++) if(reference[i].live && reference[i].rid.page==rid.page && reference[i].rid.slot==rid.slot) { found=i; break; }
        CHECK(found>=0 && !seen[found]); seen[found]=1;
        CHECK(reference[found].length==length && !memcmp(reference[found].data,data,length));
    }
    CHECK(error==RME_EOF);
    for(i=0;i<REFERENCES;i++) CHECK(seen[i]==reference[i].live);
    CHECK(RM_CloseScan(&scan)==RME_OK);
}
static void randomized(int policy)
{
    int fd,step,index,action,j;
    size_t length;
    unsigned char out[PF_PAGE_SIZE];
    struct reference *ref;
    CHECK(RM_CreateFile("random.pf")==RME_OK);
    fd=RM_OpenFile("random.pf",policy); CHECK(fd>=0);
    for(step=0;step<2500;step++) {
        index=(next_random()>>8)%REFERENCES;
        action=(next_random()>>16)%3;
        ref=&reference[index];
        if(!ref->live) {
            ref->length=1+(next_random()>>8)%sizeof(ref->data);
            for(j=0;j<(int)ref->length;j++) ref->data[j]=(unsigned char)(next_random()>>24);
            CHECK(RM_InsertRecord(fd,ref->data,ref->length,&ref->rid)==RME_OK);
            ref->live=1;
        } else if(action==0) {
            CHECK(RM_DeleteRecord(fd,ref->rid)==RME_OK); ref->live=0;
            CHECK(RM_GetRecord(fd,ref->rid,out,sizeof(out),&length)==RME_NOTFOUND);
        } else {
            CHECK(RM_GetRecord(fd,ref->rid,out,sizeof(out),&length)==RME_OK);
            CHECK(length==ref->length && !memcmp(out,ref->data,length));
        }
        if(step%41==0) check_scan(fd);
        if(step%137==0) {
            CHECK(RM_CloseFile(fd)==RME_OK);
            fd=RM_OpenFile("random.pf",policy); CHECK(fd>=0); check_scan(fd);
        }
    }
    check_scan(fd); CHECK(RM_CloseFile(fd)==RME_OK);
}
static void fixed(int policy)
{
    FM_File file,wrong;
    FM_Scan scan;
    RM_RID rid[90],current;
    RM_Space space;
    char data[FM_MAX_RECORD],out[PF_PAGE_SIZE],*page;
    size_t length;
    int i,j;
    memset(data,'x',sizeof(data));
    CHECK(FM_CreateFile("fixed.pf")==RME_OK);
    CHECK(FM_OpenFile("fixed.pf",policy,100,&file)==RME_OK);
    CHECK(FM_OpenScan(&file,&scan)==RME_OK);
    CHECK(FM_GetNextRecord(&scan,&current,out,sizeof(out),&length)==RME_EOF);
    CHECK(FM_CloseScan(&scan)==RME_OK);
    CHECK(FM_InsertRecord(&file,data,101,&current)==RME_TOOLARGE);
    CHECK(FM_InsertRecord(&file,data,0,&current)==RME_INVALID);
    for(i=0;i<90;i++) CHECK(FM_InsertRecord(&file,data,1+i%100,&rid[i])==RME_OK);
    CHECK(FM_GetRecord(&file,rid[1],out,1,&length)==RME_TOOSMALL && length==2);
    CHECK(FM_GetSpace(&file,&space)==RME_OK && space.pages==3 && space.records==90);
    CHECK(space.payload+space.metadata+space.padding+space.unused==space.pages*PF_PAGE_SIZE);
    /* Inspect padding through PF, not direct database file I/O. */
    CHECK(PF_GetThisPage(file.fd,0,&page)==PFE_OK);
    CHECK(rm_u16(page+8)==1 && rm_u16(page+10)==1);
    for(j=13;j<112;j++) CHECK(page[j]==0);
    CHECK(PF_UnfixPage(file.fd,0,FALSE)==PFE_OK);
    CHECK(FM_CloseFile(&file)==RME_OK);
    CHECK(FM_OpenFile("fixed.pf",policy,99,&wrong)==RME_CORRUPT);
    CHECK(RM_OpenFile("fixed.pf",policy)==RME_CORRUPT);
    CHECK(FM_OpenFile("fixed.pf",policy,100,&file)==RME_OK);
    CHECK(FM_OpenScan(&file,&scan)==RME_OK);
    for(i=0;i<90;i++) {
        CHECK(FM_GetNextRecord(&scan,&current,out,0,&length)==RME_TOOSMALL);
        CHECK(FM_GetNextRecord(&scan,&current,out,sizeof(out),&length)==RME_OK);
        CHECK(length==(size_t)(1+i%100) && !memcmp(out,data,length));
        CHECK(current.page==rid[i].page && current.slot==rid[i].slot);
    }
    CHECK(FM_GetNextRecord(&scan,&current,out,sizeof(out),&length)==RME_EOF);
    CHECK(FM_CloseScan(&scan)==RME_OK); CHECK(FM_CloseFile(&file)==RME_OK);
    CHECK(FM_CreateFile("maxfixed.pf")==RME_OK);
    CHECK(FM_OpenFile("maxfixed.pf",policy,FM_MAX_RECORD,&file)==RME_OK);
    CHECK(FM_InsertRecord(&file,data,sizeof(data),&current)==RME_OK);
    CHECK(FM_GetSpace(&file,&space)==RME_OK && space.unused==0 && space.padding==0);
    CHECK(FM_CloseFile(&file)==RME_OK);
}
int main(int argc,char **argv)
{
    CHECK(argc==4); PF_Init(); CHECK(PF_SetBufferSize(atoi(argv[1]))==PFE_OK);
    if(!strcmp(argv[3],"boundaries")) boundaries(atoi(argv[2]));
    else if(!strcmp(argv[3],"random")) randomized(atoi(argv[2]));
    else if(!strcmp(argv[3],"fixed")) fixed(atoi(argv[2]));
    else CHECK(0);
    printf("PASS: %s capacity=%s policy=%s seed=%u\n",argv[3],argv[1],argv[2],SEED);
    return 0;
}
