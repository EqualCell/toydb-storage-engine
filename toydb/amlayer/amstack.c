# include <stdio.h>
# include "am.h"
# include "pf.h"



struct
    {
     int pageNumber;
     int offset;
    } AM_Stack[AM_MAXSTACK];

int AM_topofStackPtr = -1;

AM_PushStack(pageNum,offset)
int pageNum;
int offset;

{
if (AM_topofStackPtr + 1 >= AM_MAXSTACK) return(AME_STACK);
AM_topofStackPtr++;
AM_Stack[AM_topofStackPtr].pageNumber  = pageNum;
AM_Stack[AM_topofStackPtr].offset  = offset;
return(AME_OK);
}

AM_PopStack()

{
if (AM_topofStackPtr < 0) return(AME_STACK);
AM_topofStackPtr--;
return(AME_OK);
}

AM_topofStack(pageNum,offset)
int *pageNum;
int *offset;
{
if (AM_topofStackPtr < 0) return(AME_STACK);
*pageNum = AM_Stack[AM_topofStackPtr].pageNumber ;
*offset = AM_Stack[AM_topofStackPtr].offset ;
return(AME_OK);
}

AM_EmptyStack()

{
AM_topofStackPtr = -1;
return(AME_OK);
}

