#ifndef TOYDB_INDEX_H
#define TOYDB_INDEX_H
#include "../amlayer/am.h"
#include "../rmlayer/rm.h"
#include "../rmlayer/student.h"
#define IX_INVALID -201
#define IX_RM -202
#define IX_AM -203
#define IX_NOMEM -204
extern int IX_LastError;
int IX_Key(const void *record,size_t size,int column,int length,char key[255]);
int IX_Encode(RM_RID rid,int *encoded);
int IX_Decode(int encoded,RM_RID *rid);
int IX_Collect(int records,int column,int length,AM_KeyRID **pairs,size_t *count);
void IX_Sort(AM_KeyRID *pairs,size_t count);
int IX_InsertPairs(int index,int length,const AM_KeyRID *pairs,size_t count);
int IX_BuildExisting(int records,int index,int column,int length);
int IX_Incremental(int records,int index,const StudentData *data,int column,int length,AM_KeyRID *reference);
#endif
