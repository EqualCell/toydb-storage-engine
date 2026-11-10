#include "index.h"

int IX_LastError;
int IX_Key(const void *record,size_t size,int column,int length,char key[255])
{
    const char *bytes=record;
    size_t i,start=0;
    int field=1;
    if(!record || !key || column<1 || length<1 || length>255)return IX_INVALID;
    for(i=0;i<=size;i++) if(i==size || bytes[i]==';') {
        if(field==column) {
            if(i-start!=(size_t)length || memchr(bytes+start,0,i-start))return IX_INVALID;
            memset(key,0,255);memcpy(key,bytes+start,length);return 0;
        }
        field++;start=i+1;
    }
    return IX_INVALID;
}
int IX_Encode(RM_RID rid,int *encoded)
{
    if(!encoded || rid.page<0 || rid.slot<0 || rid.slot>=1024 || rid.page>(INT_MAX-rid.slot)/1024)
        return IX_INVALID;
    *encoded=rid.page*1024+rid.slot;return 0;
}
int IX_Decode(int encoded,RM_RID *rid)
{
    if(encoded<0 || !rid)return IX_INVALID;
    rid->page=encoded/1024;rid->slot=encoded%1024;return 0;
}
int IX_Collect(int records,int column,int length,AM_KeyRID **pairs,size_t *count)
{
    RM_Scan scan;
    RM_RID rid;
    char record[PF_PAGE_SIZE];
    size_t size,n=0,capacity=0;
    AM_KeyRID *list=NULL,*grown;
    int error;
    if(!pairs || !count)return IX_INVALID;
    error=RM_OpenScan(records,&scan);if(error){IX_LastError=error;return IX_RM;}
    while((error=RM_GetNextRecord(&scan,&rid,record,sizeof(record),&size))==0) {
        if(n==capacity) {
            if(capacity>(size_t)INT_MAX/2/sizeof(*list)){error=IX_NOMEM;goto fail;}
            capacity=capacity ? capacity*2 : 128;
            grown=realloc(list,capacity*sizeof(*list));
            if(!grown){error=IX_NOMEM;goto fail;}list=grown;
        }
        error=IX_Key(record,size,column,length,list[n].key);if(error)goto fail;
        error=IX_Encode(rid,&list[n].recId);if(error)goto fail;
        n++;
    }
    if(error!=RME_EOF){IX_LastError=error;error=IX_RM;goto fail;}
    RM_CloseScan(&scan);*pairs=list;*count=n;return 0;
fail:
    RM_CloseScan(&scan);free(list);return error;
}
static int compare_pair(const void *left,const void *right)
{
    const AM_KeyRID *a=left,*b=right;
    int order=strncmp(a->key,b->key,255);
    return order ? order : (a->recId>b->recId)-(a->recId<b->recId);
}
void IX_Sort(AM_KeyRID *pairs,size_t count)
{
    if(count)qsort(pairs,count,sizeof(*pairs),compare_pair);
}
int IX_InsertPairs(int index,int length,const AM_KeyRID *pairs,size_t count)
{
    size_t i;
    int error;
    for(i=0;i<count;i++) {
        error=AM_InsertEntry(index,'c',length,(char *)pairs[i].key,pairs[i].recId);
        if(error){IX_LastError=error;return IX_AM;}
    }
    return 0;
}
int IX_BuildExisting(int records,int index,int column,int length)
{
    AM_KeyRID *pairs;
    size_t count;
    int error=IX_Collect(records,column,length,&pairs,&count);
    if(error)return error;
    error=IX_InsertPairs(index,length,pairs,count);free(pairs);return error;
}
int IX_Incremental(int records,int index,const StudentData *data,int column,int length,AM_KeyRID *reference)
{
    size_t i;
    RM_RID rid;
    char key[255];
    int encoded,error;
    /* Preflight every key before making any persistent change. */
    for(i=0;i<data->count;i++)if(IX_Key(data->lines[i].bytes,data->lines[i].length,column,length,key))return IX_INVALID;
    for(i=0;i<data->count;i++) {
        error=RM_InsertRecord(records,data->lines[i].bytes,data->lines[i].length,&rid);
        if(error){IX_LastError=error;return IX_RM;}
        IX_Key(data->lines[i].bytes,data->lines[i].length,column,length,key);
        if(IX_Encode(rid,&encoded))return IX_INVALID;
        if(reference){memcpy(reference[i].key,key,255);reference[i].recId=encoded;}
        error=AM_InsertEntry(index,'c',length,key,encoded);
        if(error){IX_LastError=error;return IX_AM;}
    }
    return 0;
}
