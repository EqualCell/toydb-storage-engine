#include "am.h"

/* Scans own cursors, never insertion paths or fixed frames. */
static struct {
    int active, fd, op, length, page, index, haveLast;
    char type, value[255], last[255];
    short nextRec;
} scans[MAXSCANS];

int GetLeftPageNum(int fd)
{
    int number, next, error;
    char *page;
    error = PF_GetFirstPage(fd,&number,&page);
    if (error != PFE_OK) return(AME_PF);
    while (page[0] != 'l') {
        if (page[0] != 'i') { PF_UnfixPage(fd,number,FALSE); return(AME_INTERROR); }
        memcpy(&next,page+AM_sint,AM_si);
        error = PF_UnfixPage(fd,number,FALSE);
        if (error != PFE_OK) return(AME_PF);
        number = next;
        error = PF_GetThisPage(fd,number,&page);
        if (error != PFE_OK) return(AME_PF);
    }
    error = PF_UnfixPage(fd,number,FALSE);
    return error == PFE_OK ? number : AME_PF;
}

AM_OpenIndexScan(fd,type,length,op,value)
int fd, length, op;
char type, *value;
{
    int s, number, index = 1, status, error;
    char *page;
    AM_LEAFHEADER header;
    AM_EmptyStack();
    if (type!='c' && type!='i' && type!='f') return(AME_INVALIDATTRTYPE);
    if (length<1 || length>255 || (type!='c' && length!=4)) return(AME_INVALIDATTRLENGTH);
    if (value && (op<EQUAL || op>NOT_EQUAL)) return(AME_INVALID_OP_TO_SCAN);
    for (s=0;s<MAXSCANS;s++) if (!scans[s].active) break;
    if (s==MAXSCANS) return(AME_SCAN_TAB_FULL);
    if (value && (op==EQUAL || op==GREATER_THAN || op==GREATER_THAN_EQUAL)) {
        status = AM_Search(fd,type,length,value,&number,&page,&index);
        AM_EmptyStack(); /* A read-only search has no split propagation to do. */
        if (status<0) return(status);
        error = PF_UnfixPage(fd,number,FALSE);
        if (error!=PFE_OK) return(AME_PF);
    } else {
        number = GetLeftPageNum(fd);
        if (number<0) return(number);
    }
    error = PF_GetThisPage(fd,number,&page);
    if (error!=PFE_OK) return(AME_PF);
    memcpy(&header,page,AM_sl);
    error = PF_UnfixPage(fd,number,FALSE);
    if (error!=PFE_OK) return(AME_PF);
    if (header.attrLength!=length) return(AME_INVALIDATTRLENGTH);
    memset(&scans[s],0,sizeof(scans[s]));
    scans[s].fd=fd; scans[s].type=type; scans[s].length=length;
    scans[s].op=value ? op : ALL; scans[s].page=number; scans[s].index=index;
    if (value) memcpy(scans[s].value,value,length);
    scans[s].active=TRUE;
    return(s);
}

static int accepts(int op, int comparison)
{
    /* AM_Compare returns query compared with stored key. */
    switch (op) {
    case ALL: return TRUE;
    case EQUAL: return comparison==0;
    case LESS_THAN: return comparison>0;
    case GREATER_THAN: return comparison<0;
    case LESS_THAN_EQUAL: return comparison>=0;
    case GREATER_THAN_EQUAL: return comparison<=0;
    case NOT_EQUAL: return comparison!=0;
    }
    return FALSE;
}

int AM_FindNextEntry(int s)
{
    int fd, number, index, status, comparison, reference, error, stride;
    short cell, following;
    char *page, *key;
    AM_LEAFHEADER header;
    if (s<0 || s>=MAXSCANS || !scans[s].active) return(AME_INVALID_SCANDESC);
    fd=scans[s].fd;
    while ((number=scans[s].page)!=AM_NULL_PAGE) {
        error=PF_GetThisPage(fd,number,&page);
        if (error!=PFE_OK) return(AME_PF);
        memcpy(&header,page,AM_sl);
        stride=header.attrLength+AM_ss;
        index=scans[s].index;
        cell=0;
        if (scans[s].haveLast) {
            /* Locate the last key again: supplied tests delete during scans. */
            status=AM_SearchLeaf(page,scans[s].type,scans[s].length,scans[s].last,&index,&header);
            if (status==AM_FOUND) {
                cell=scans[s].nextRec;
                if (!cell) index++;
            }
        }
        while (index<=header.numKeys) {
            key=page+AM_sl+(index-1)*stride;
            comparison=scans[s].op==ALL ? 0 : AM_Compare(key,scans[s].type,scans[s].length,scans[s].value);
            if (accepts(scans[s].op,comparison)) {
                if (!cell) memcpy(&cell,key+header.attrLength,AM_ss);
                if (cell<AM_sl || cell+AM_si+AM_ss>AM_NODE_SIZE) {
                    PF_UnfixPage(fd,number,FALSE); return(AME_INTERROR);
                }
                memcpy(&reference,page+cell,AM_si);
                memcpy(&following,page+cell+AM_si,AM_ss);
                memcpy(scans[s].last,key,header.attrLength);
                scans[s].haveLast=TRUE; scans[s].nextRec=following; scans[s].index=index;
                error=PF_UnfixPage(fd,number,FALSE);
                return error==PFE_OK ? reference : AME_PF;
            }
            /* These predicates cannot match any later key once past the bound. */
            if ((scans[s].op==EQUAL && comparison<0) ||
                (scans[s].op==LESS_THAN && comparison<=0) ||
                (scans[s].op==LESS_THAN_EQUAL && comparison<0)) {
                scans[s].page=AM_NULL_PAGE;
                error=PF_UnfixPage(fd,number,FALSE);
                return error==PFE_OK ? AME_EOF : AME_PF;
            }
            index++; cell=0;
        }
        scans[s].page=header.nextLeafPage;
        scans[s].index=1; scans[s].haveLast=FALSE;
        error=PF_UnfixPage(fd,number,FALSE);
        if (error!=PFE_OK) return(AME_PF);
    }
    return(AME_EOF);
}

int AM_CloseIndexScan(int s)
{
    if (s<0 || s>=MAXSCANS || !scans[s].active) return(AME_INVALID_SCANDESC);
    scans[s].active=FALSE;
    AM_EmptyStack();
    return(AME_OK);
}
