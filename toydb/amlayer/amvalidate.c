#include "am.h"

typedef struct {
    int fd,length,limit,height,pages,lastLeaf,lastNext;
    size_t entries;
    unsigned char *seen;
} Validation;
static int visit(Validation *v,int number,int depth,int root,char *minimum,char *maximum)
{
    char *page,copy[AM_NODE_SIZE],childMin[255],childMax[255],previous[255];
    unsigned char used[AM_NODE_SIZE]={0};
    AM_LEAFHEADER leaf;
    AM_INTHEADER node;
    int i,j,child,reference,error;
    short cell,next;
    if (number<0 || number>=v->limit || v->seen[number] || depth>AM_MAXSTACK) return AME_INTERROR;
    v->seen[number]=1;v->pages++;
    if (PF_GetThisPage(v->fd,number,&page)!=PFE_OK) return AME_PF;
    memcpy(copy,page,AM_NODE_SIZE);
    if(PF_UnfixPage(v->fd,number,FALSE)!=PFE_OK)return AME_PF;
    if(copy[0]=='l') {
        memcpy(&leaf,copy,AM_sl);
        if(leaf.attrLength!=v->length || leaf.numKeys<0 || (!root && !leaf.numKeys) ||
           leaf.keyPtr!=AM_sl+leaf.numKeys*(v->length+AM_ss) || leaf.keyPtr>leaf.recIdPtr || leaf.recIdPtr>AM_NODE_SIZE)
            return AME_INTERROR;
        if(v->height && depth!=v->height)return AME_INTERROR;
        v->height=depth;
        if(v->lastLeaf>=0 && v->lastNext!=number)return AME_INTERROR;
        v->lastLeaf=number;v->lastNext=leaf.nextLeafPage;
        for(i=0;i<leaf.numKeys;i++) {
            char *key=copy+AM_sl+i*(v->length+AM_ss);
            if(i && strncmp(key-(v->length+AM_ss),key,v->length)>=0)return AME_INTERROR;
            memcpy(&cell,key+v->length,AM_ss);
            if(!cell)return AME_INTERROR;
            while(cell) {
                if(cell<leaf.keyPtr || cell+AM_si+AM_ss>AM_NODE_SIZE)return AME_INTERROR;
                for(j=cell;j<cell+(int)(AM_si+AM_ss);j++) if(used[j]++)return AME_INTERROR;
                memcpy(&reference,copy+cell,AM_si); if(reference<0)return AME_INTERROR;
                v->entries++;memcpy(&next,copy+cell+AM_si,AM_ss);cell=next;
            }
        }
        if(leaf.numKeys) {
            memcpy(minimum,copy+AM_sl,v->length);
            memcpy(maximum,copy+AM_sl+(leaf.numKeys-1)*(v->length+AM_ss),v->length);
        } else {memset(minimum,0,v->length);memset(maximum,0,v->length);}
        return AME_OK;
    }
    if(copy[0]!='i')return AME_INTERROR;
    memcpy(&node,copy,AM_sint);
    if(node.attrLength!=v->length || node.numKeys<1 || node.numKeys>node.maxKeys ||
       (!root && node.numKeys<node.maxKeys/2) ||
       AM_sint+AM_si+node.numKeys*(v->length+AM_si)>AM_NODE_SIZE)return AME_INTERROR;
    for(i=0;i<=node.numKeys;i++) {
        memcpy(&child,copy+AM_sint+i*(v->length+AM_si),AM_si);
        error=visit(v,child,depth+1,FALSE,childMin,childMax);if(error)return error;
        if(!i)memcpy(minimum,childMin,v->length);
        else {
            if(strncmp(previous,childMin,v->length)>=0 ||
               strncmp(childMin,copy+AM_sint+AM_si+(i-1)*(v->length+AM_si),v->length))return AME_INTERROR;
        }
        memcpy(previous,childMax,v->length);
    }
    memcpy(maximum,previous,v->length);return AME_OK;
}
int AM_ValidateTree(int fd,int length,size_t expected,int *height,int *pages)
{
    Validation v;
    int number=-1,error;
    char *page,minimum[255],maximum[255];
    memset(&v,0,sizeof(v));v.fd=fd;v.length=length;v.lastLeaf=-1;
    if(length<1 || length>255)return AME_INVALIDATTRLENGTH;
    while((error=PF_GetNextPage(fd,&number,&page))==PFE_OK) {
        v.limit=number+1;
        if(PF_UnfixPage(fd,number,FALSE)!=PFE_OK)return AME_PF;
    }
    if(error!=PFE_EOF || !v.limit)return AME_PF;
    v.seen=calloc(v.limit,1);if(!v.seen)return AME_NOMEM;
    error=visit(&v,0,1,TRUE,minimum,maximum);
    if(!error && (v.entries!=expected || v.lastNext!=-1 || v.pages!=v.limit))error=AME_INTERROR;
    free(v.seen);
    if(!error){*height=v.height;*pages=v.pages;}
    return error;
}
