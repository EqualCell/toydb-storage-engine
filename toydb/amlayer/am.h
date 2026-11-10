#ifndef TOYDB_AM_H
#define TOYDB_AM_H
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "pf.h"
#define AM_NODE_SIZE 1020
#define AM_MAXSTACK 50

typedef struct am_leafheader
	{
		char pageType;
		int nextLeafPage;
		short recIdPtr;
		short keyPtr;
		short freeListPtr;
		short numinfreeList;
		short attrLength;
		short numKeys;
		short maxKeys;
	}  AM_LEAFHEADER; /* Header for a leaf page */

typedef struct am_intheader 
	{
		char pageType;
		short numKeys;
		short maxKeys;
		short attrLength;
	}	AM_INTHEADER ; /* Header for an internal node */

extern int AM_RootPageNum; /* The page number of the root */
extern int AM_LeftPageNum; /* The page Number of the leftmost leaf */
extern int AM_Errno; /* last error in AM layer */


# define AM_Check if (errVal != PFE_OK) {AM_EmptyStack(); AM_Errno = AME_PF; return(AME_PF) ;}
# define AM_si sizeof(int)
# define AM_ss sizeof(short)
# define AM_sl sizeof(AM_LEAFHEADER)
# define AM_sint sizeof(AM_INTHEADER)
# define AM_sc sizeof(char)
# define AM_sf sizeof(float)
# define AM_NOT_FOUND 0 /* Key is not in tree */
# define AM_FOUND 1 /* Key is in tree */
# define AM_NULL 0 /* Null pointer for lists in a page */
# define AM_MAX_FNAME_LENGTH 80
# define AM_NULL_PAGE -1 
# define FREE 0 /* Free is chosen to be zero because C initialises all 
	   variablesto zero and we require that our scan table be initialised */
# define FIRST 1 
# define BUSY 2
# define LAST 3
# define OVER 4
# define ALL 0
# define EQUAL 1
# define LESS_THAN 2
# define GREATER_THAN 3
# define LESS_THAN_EQUAL 4
# define GREATER_THAN_EQUAL 5
# define NOT_EQUAL 6
# define MAXSCANS 20
# define AM_MAXATTRLENGTH 256


# define AME_OK 0
# define AME_INVALIDATTRLENGTH -1
# define AME_NOTFOUND -2
# define AME_PF -3
# define AME_INTERROR -4
# define AME_INVALID_SCANDESC -5
# define AME_INVALID_OP_TO_SCAN -6
# define AME_EOF -7
# define AME_SCAN_TAB_FULL -8
# define AME_INVALIDATTRTYPE -9
# define AME_FD -10
# define AME_INVALIDVALUE -11

#define AME_STACK -12
#define AME_DUPLIMIT -13
#define AME_NOMEM -14

/* Legacy functions use promoted char/short arguments. */
extern int AM_CreateIndex();
extern int AM_DestroyIndex();
extern int AM_InsertEntry();
extern int AM_DeleteEntry();
extern int AM_OpenIndexScan();
extern int AM_FindNextEntry();
extern int AM_CloseIndexScan();
extern int AM_EmptyStack();
extern int AM_PushStack();
extern int AM_PopStack();
extern int AM_topofStack();
extern int PF_CreateFile();
extern int PF_DestroyFile();
extern int PF_CloseFile();
extern int PF_GetFirstPage();
extern int PF_GetNextPage();
extern int PF_GetThisPage();
extern int PF_AllocPage();
extern int PF_UnfixPage();
extern int PF_DisposePage();

typedef struct AM_KeyRID {
    char key[255];
    int recId;
} AM_KeyRID;
int AM_BulkLoad(int fd, int length, const AM_KeyRID *pairs, size_t count);
int AM_ValidateTree(int fd, int length, size_t expected, int *height, int *pages);
#endif
