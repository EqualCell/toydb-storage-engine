#ifndef TOYDB_FIXED_H
#define TOYDB_FIXED_H
#include "rm.h"
#define FM_HEADER_SIZE 8
#define FM_SLOT_META 4
#define FM_MAX_RECORD (PF_PAGE_SIZE - FM_HEADER_SIZE - FM_SLOT_META)
typedef struct FM_File { int fd; unsigned length; } FM_File;
typedef struct FM_Scan { RM_Scan cursor; unsigned length; } FM_Scan;
int FM_CreateFile(char *name);
int FM_OpenFile(char *name, int policy, unsigned length, FM_File *file);
int FM_CloseFile(FM_File *file);
int FM_InsertRecord(FM_File *file, const void *data, size_t length, RM_RID *rid);
int FM_GetRecord(FM_File *file, RM_RID rid, void *out, size_t capacity, size_t *length);
int FM_OpenScan(FM_File *file, FM_Scan *scan);
int FM_GetNextRecord(FM_Scan *scan, RM_RID *rid, void *out, size_t capacity, size_t *length);
int FM_CloseScan(FM_Scan *scan);
int FM_GetSpace(FM_File *file, RM_Space *space);
#endif
