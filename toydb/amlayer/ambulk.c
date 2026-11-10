#include "am.h"

typedef struct { size_t first, end; int bytes; } Group;
typedef struct { size_t first, end; int bytes, page; } Leaf;
typedef struct { int page; char minimum[255]; } Child;

static int release_page(int fd,int number,int dirty)
{
    int a=0,b;
    if (dirty) a=PF_MarkDirty(fd,number);
    b=PF_UnfixPage(fd,number,dirty);
    return a==PFE_OK && b==PFE_OK ? AME_OK : AME_PF;
}
static int allocate_page(int fd,int *number)
{
    char *page;
    if (PF_AllocPage(fd,number,&page)!=PFE_OK) return AME_PF;
    memset(page,0,AM_NODE_SIZE);
    return release_page(fd,*number,TRUE);
}
static int max_keys(int length)
{
    int n=(AM_NODE_SIZE-AM_sint-AM_si)/(length+AM_si);
    return n-n%2;
}
static int fill_leaf(int fd,int number,int next,int length,const AM_KeyRID *pairs,
                     const Group *groups,size_t first,size_t end)
{
    char *page,*entry;
    AM_LEAFHEADER h;
    size_t i,j;
    short cell,head;
    if (PF_GetThisPage(fd,number,&page)!=PFE_OK) return AME_PF;
    memset(page,0,AM_NODE_SIZE); memset(&h,0,sizeof(h));
    h.pageType='l'; h.nextLeafPage=next; h.recIdPtr=AM_NODE_SIZE;
    h.attrLength=length; h.maxKeys=max_keys(length); h.numKeys=end-first;
    h.keyPtr=AM_sl+h.numKeys*(length+AM_ss);
    for (i=first;i<end;i++) {
        entry=page+AM_sl+(i-first)*(length+AM_ss);
        memcpy(entry,pairs[groups[i].first].key,length); head=0;
        for (j=groups[i].first;j<groups[i].end;j++) {
            h.recIdPtr-=AM_si+AM_ss; cell=h.recIdPtr;
            memcpy(page+cell,&pairs[j].recId,AM_si);
            memcpy(page+cell+AM_si,&head,AM_ss); head=cell;
        }
        memcpy(entry+length,&head,AM_ss);
    }
    memcpy(page,&h,AM_sl);
    return release_page(fd,number,TRUE);
}
static int fill_parent(int fd,int number,int length,const Child *children,size_t count)
{
    char *page;
    AM_INTHEADER h;
    size_t i;
    if (PF_GetThisPage(fd,number,&page)!=PFE_OK) return AME_PF;
    memset(page,0,AM_NODE_SIZE); memset(&h,0,sizeof(h));
    h.pageType='i'; h.numKeys=count-1; h.maxKeys=max_keys(length); h.attrLength=length;
    memcpy(page,&h,AM_sint);
    for (i=0;i<count;i++) {
        memcpy(page+AM_sint+i*(length+AM_si),&children[i].page,AM_si);
        /* Separator equals the minimum key in the right child. */
        if (i) memcpy(page+AM_sint+AM_si+(i-1)*(length+AM_si),children[i].minimum,length);
    }
    return release_page(fd,number,TRUE);
}
int AM_BulkLoad(int fd,int length,const AM_KeyRID *pairs,size_t count)
{
    Group *groups=NULL;
    Leaf *leaves=NULL;
    Child *children=NULL,*parents=NULL;
    size_t ng=0,nl=0,i,j,start,n,level,parts,base,extra,take,k;
    int error=AME_OK,number=-1,capacity=AM_NODE_SIZE-AM_sl,minbytes,bytes,left,right,best,cut;
    char *page;
    AM_LEAFHEADER h;
    if (length<1 || length>255 || (!pairs && count) || count>(size_t)INT_MAX/sizeof(Group))
        return AME_INVALIDVALUE;
    /* Validate input and plan groups before changing the empty index. */
    groups=calloc(count ? count : 1,sizeof(*groups));
    leaves=calloc(count ? count : 1,sizeof(*leaves));
    if (!groups || !leaves) { error=AME_NOMEM; goto done; }
    for (i=0;i<count;) {
        start=i;
        if (pairs[i].recId<0 || (i && strncmp(pairs[i-1].key,pairs[i].key,length)>0)) {
            error=AME_INVALIDVALUE; goto done;
        }
        do {
            if (pairs[i].recId<0) { error=AME_INVALIDVALUE; goto done; }
            i++;
        } while(i<count && !strncmp(pairs[start].key,pairs[i].key,length));
        n=i-start;
        if (n>(size_t)(capacity-length-AM_ss)/(AM_si+AM_ss)) { error=AME_DUPLIMIT; goto done; }
        groups[ng].first=start; groups[ng].end=i;
        groups[ng++].bytes=length+AM_ss+n*(AM_si+AM_ss);
    }
    for (i=0;i<ng;) {
        leaves[nl].first=i; bytes=0;
        while (i<ng && bytes+groups[i].bytes<=capacity) bytes+=groups[i++].bytes;
        leaves[nl].end=i; leaves[nl++].bytes=bytes;
    }
    minbytes=((capacity/(length+AM_ss+AM_si+AM_ss))/2)*(length+AM_ss+AM_si+AM_ss);
    /* Balance neighboring leaves, especially a short final page. Groups stay whole. */
    for (i=0;nl>1 && i<nl;i++) if (leaves[i].bytes<minbytes) {
        j=i ? i-1 : 0; bytes=leaves[j].bytes+leaves[j+1].bytes;
        left=0; best=INT_MAX; cut=-1;
        for (k=leaves[j].first;k+1<leaves[j+1].end;k++) {
            left+=groups[k].bytes; right=bytes-left;
            if (left>=minbytes && right>=minbytes && left<=capacity && right<=capacity && abs(left-right)<best) {
                best=abs(left-right); cut=k+1;
            }
        }
        if (cut<0) { error=AME_DUPLIMIT; goto done; }
        leaves[j].end=cut; leaves[j+1].first=cut;
        left=0; for(k=leaves[j].first;k<leaves[j].end;k++) left+=groups[k].bytes;
        leaves[j].bytes=left; leaves[j+1].bytes=bytes-left;
    }
    if (PF_GetFirstPage(fd,&number,&page)!=PFE_OK) { error=AME_PF; goto done; }
    memcpy(&h,page,AM_sl);
    error=release_page(fd,number,FALSE);
    if (error) goto done;
    if (number!=0 || h.pageType!='l' || h.numKeys || h.attrLength!=length) { error=AME_INVALIDVALUE; goto done; }
    error=PF_GetNextPage(fd,&number,&page);
    if (error!=PFE_EOF) {
        if (error==PFE_OK) release_page(fd,number,FALSE);
        error=error==PFE_OK ? AME_INVALIDVALUE : AME_PF; goto done;
    }
    error=AME_OK;
    if (nl<=1) { error=fill_leaf(fd,0,-1,length,pairs,groups,0,ng); goto done; }
    children=calloc(nl,sizeof(*children));
    if (!children) { error=AME_NOMEM; goto done; }
    for (i=0;i<nl;i++) {
        error=allocate_page(fd,&leaves[i].page); if(error)goto done;
        children[i].page=leaves[i].page;
        memcpy(children[i].minimum,pairs[groups[leaves[i].first].first].key,length);
    }
    for (i=0;i<nl;i++) {
        error=fill_leaf(fd,leaves[i].page,i+1<nl ? leaves[i+1].page : -1,length,pairs,groups,leaves[i].first,leaves[i].end);
        if(error)goto done;
    }
    level=nl;
    while (level>(size_t)max_keys(length)+1) {
        parts=(level+max_keys(length))/(max_keys(length)+1);
        base=level/parts; extra=level%parts;
        parents=calloc(parts,sizeof(*parents));
        if(!parents){error=AME_NOMEM;goto done;}
        start=0;
        for (i=0;i<parts;i++) {
            take=base+(i<extra);
            error=allocate_page(fd,&parents[i].page); if(error)goto done;
            memcpy(parents[i].minimum,children[start].minimum,length);
            error=fill_parent(fd,parents[i].page,length,children+start,take); if(error)goto done;
            start+=take;
        }
        free(children); children=parents; parents=NULL; level=parts;
    }
    error=fill_parent(fd,0,length,children,level);
done:
    free(groups); free(leaves); free(children); free(parents);
    AM_EmptyStack();
    if(error) AM_Errno=error;
    return error;
}
