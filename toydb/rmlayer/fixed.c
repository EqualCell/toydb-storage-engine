#include "fixed.h"
#include "internal.h"
/* Header: magic[4], maximum length:u16, used slot count:u16.
   Each used slot: actual length:u16, live state:u16, L payload bytes. */
static char *fixed_slot(char *page, unsigned slot, unsigned length)
{
    return page + FM_HEADER_SIZE + slot * (FM_SLOT_META + length);
}
static int fixed_valid(char *page, unsigned length)
{
    unsigned count = rm_u16(page + 6), i;
    char *slot;
    if (!length || length > FM_MAX_RECORD || memcmp(page,"FMP1",4) ||
        rm_u16(page+4)!=length || count > (PF_PAGE_SIZE-FM_HEADER_SIZE)/(length+FM_SLOT_META))
        return RME_CORRUPT;
    for (i=0;i<count;i++) {
        slot=fixed_slot(page,i,length);
        if (!rm_u16(slot) || rm_u16(slot)>length || rm_u16(slot+2)!=1) return RME_CORRUPT;
    }
    return RME_OK;
}
int FM_CreateFile(char *name) { return RM_CreateFile(name); }
int FM_OpenFile(char *name, int policy, unsigned length, FM_File *file)
{
    int fd, number=-1, error, saved;
    char *page;
    if (!file || !length) return rm_error(RME_INVALID);
    if (length>FM_MAX_RECORD) return rm_error(RME_TOOLARGE);
    fd=rm_open(name,policy,"FMP1");
    if (fd<0) return fd;
    error=PF_GetNextPage(fd,&number,&page);
    if (error==PFE_OK) error=rm_release(fd,number,FALSE,fixed_valid(page,length));
    else if (error==PFE_EOF) error=RME_OK;
    else error=RME_PF;
    if (error!=RME_OK) {
        saved=PFerrno; PF_CloseFile(fd); PFerrno=saved; return rm_error(error);
    }
    file->fd=fd; file->length=length;
    return RME_OK;
}
int FM_CloseFile(FM_File *file)
{
    int error;
    if (!file) return rm_error(RME_INVALID);
    error=RM_CloseFile(file->fd);
    if (error==RME_OK) file->fd=-1;
    return error;
}
int FM_InsertRecord(FM_File *file, const void *data, size_t length, RM_RID *rid)
{
    int number=-1, error;
    unsigned count;
    char *page, *slot;
    if (!file || !data || !rid || !length || !file->length || file->length>FM_MAX_RECORD)
        return rm_error(RME_INVALID);
    if (length>file->length) return rm_error(RME_TOOLARGE);
    while ((error=PF_GetNextPage(file->fd,&number,&page))==PFE_OK) {
        error=fixed_valid(page,file->length);
        if (error!=RME_OK) return rm_release(file->fd,number,FALSE,error);
        count=rm_u16(page+6);
        if (count<(PF_PAGE_SIZE-FM_HEADER_SIZE)/(file->length+FM_SLOT_META)) break;
        error=rm_release(file->fd,number,FALSE,RME_OK);
        if (error!=RME_OK) return error;
    }
    if (error==PFE_EOF) {
        error=PF_AllocPage(file->fd,&number,&page);
        if (error!=PFE_OK) return rm_pf(error);
        memset(page,0,PF_PAGE_SIZE); memcpy(page,"FMP1",4); rm_put16(page+4,file->length);
    } else if (error!=PFE_OK) return rm_pf(error);
    count=rm_u16(page+6);
    slot=fixed_slot(page,count,file->length);
    rm_put16(slot,(unsigned)length); rm_put16(slot+2,1);
    memset(slot+FM_SLOT_META,0,file->length);
    memcpy(slot+FM_SLOT_META,data,length);
    rm_put16(page+6,count+1);
    error=rm_release(file->fd,number,TRUE,RME_OK);
    if (error==RME_OK) { rid->page=number; rid->slot=count; }
    return error;
}
int FM_GetRecord(FM_File *file, RM_RID rid, void *out, size_t capacity, size_t *length)
{
    char *page,*slot;
    int error;
    if (!file || !length || (!out && capacity)) return rm_error(RME_INVALID);
    if (rid.page<0 || rid.slot<0) return rm_error(RME_NOTFOUND);
    error=PF_GetThisPage(file->fd,rid.page,&page);
    if (error==PFE_INVALIDPAGE) return rm_error(RME_NOTFOUND);
    if (error!=PFE_OK) return rm_pf(error);
    error=fixed_valid(page,file->length);
    if (error!=RME_OK) return rm_release(file->fd,rid.page,FALSE,error);
    if ((unsigned)rid.slot>=rm_u16(page+6)) return rm_release(file->fd,rid.page,FALSE,RME_NOTFOUND);
    slot=fixed_slot(page,rid.slot,file->length);
    *length=rm_u16(slot);
    if (capacity<*length) return rm_release(file->fd,rid.page,FALSE,RME_TOOSMALL);
    memcpy(out,slot+FM_SLOT_META,*length);
    return rm_release(file->fd,rid.page,FALSE,RME_OK);
}
int FM_OpenScan(FM_File *file, FM_Scan *scan)
{
    if (!file || !scan) return rm_error(RME_INVALID);
    scan->length=file->length;
    return rm_scan_open(file->fd,&scan->cursor);
}
int FM_CloseScan(FM_Scan *scan)
{
    if (!scan) return rm_error(RME_INVALID);
    return RM_CloseScan(&scan->cursor);
}
int FM_GetNextRecord(FM_Scan *scan, RM_RID *rid, void *out, size_t capacity, size_t *length)
{
    int number,error;
    RM_Scan *cursor;
    char *page,*slot;
    if (!scan || !scan->cursor.active) return rm_error(RME_SCAN);
    if (!rid || !length || (!out && capacity)) return rm_error(RME_INVALID);
    cursor=&scan->cursor;
    if (cursor->ended) return rm_error(RME_EOF);
    number=cursor->page;
    error=number<0 ? PF_GetNextPage(cursor->fd,&number,&page) : PF_GetThisPage(cursor->fd,number,&page);
    for (;;) {
        if (error==PFE_EOF) { cursor->ended=TRUE; return rm_error(RME_EOF); }
        if (error!=PFE_OK) return rm_pf(error);
        error=fixed_valid(page,scan->length);
        if (error!=RME_OK) return rm_release(cursor->fd,number,FALSE,error);
        cursor->page=number;
        if ((unsigned)cursor->slot<rm_u16(page+6)) {
            slot=fixed_slot(page,cursor->slot,scan->length);
            *length=rm_u16(slot);
            if (capacity<*length) return rm_release(cursor->fd,number,FALSE,RME_TOOSMALL);
            memcpy(out,slot+FM_SLOT_META,*length);
            error=rm_release(cursor->fd,number,FALSE,RME_OK);
            if (error!=RME_OK) return error;
            rid->page=number; rid->slot=cursor->slot++;
            return RME_OK;
        }
        error=rm_release(cursor->fd,number,FALSE,RME_OK);
        if (error!=RME_OK) return error;
        error=PF_GetNextPage(cursor->fd,&number,&page);
        if (error==PFE_OK) cursor->slot=0;
    }
}
int FM_GetSpace(FM_File *file, RM_Space *space)
{
    RM_Space result={0};
    int number=-1,error;
    unsigned count,i,payload;
    char *page;
    if (!file || !space) return rm_error(RME_INVALID);
    while ((error=PF_GetNextPage(file->fd,&number,&page))==PFE_OK) {
        error=fixed_valid(page,file->length);
        if (error!=RME_OK) return rm_release(file->fd,number,FALSE,error);
        count=rm_u16(page+6); payload=0;
        for (i=0;i<count;i++) payload+=rm_u16(fixed_slot(page,i,file->length));
        result.pages++; result.records+=count; result.payload+=payload;
        result.metadata+=FM_HEADER_SIZE+count*FM_SLOT_META;
        result.padding+=count*file->length-payload;
        result.unused+=PF_PAGE_SIZE-FM_HEADER_SIZE-count*(FM_SLOT_META+file->length);
        error=rm_release(file->fd,number,FALSE,RME_OK);
        if (error!=RME_OK) return error;
    }
    if (error!=PFE_EOF) return rm_pf(error);
    *space=result; return RME_OK;
}
