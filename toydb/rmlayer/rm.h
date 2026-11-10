#ifndef TOYDB_RM_H
#define TOYDB_RM_H
#include <stddef.h>
#include "../pflayer/pf.h"

#define RME_OK 0
#define RME_PF -101
#define RME_INVALID -102
#define RME_TOOLARGE -103
#define RME_NOTFOUND -104
#define RME_TOOSMALL -105
#define RME_EOF -106
#define RME_CORRUPT -107
#define RME_SCAN -108

#define RM_HEADER_SIZE 8
#define RM_SLOT_SIZE 6
#define RM_MAX_RECORD (PF_PAGE_SIZE - RM_HEADER_SIZE - RM_SLOT_SIZE)

typedef struct RM_RID { int page; int slot; } RM_RID;
typedef struct RM_Scan {
    int fd;
    int page;
    int slot;
    int active;
    int ended;
} RM_Scan;

typedef struct RM_Space {
    unsigned long long pages, records, payload, metadata, padding, unused;
} RM_Space;

extern int RMerrno;
int RM_CreateFile(char *name);
int RM_OpenFile(char *name, int policy); /* returns PF descriptor or RM error */
int RM_CloseFile(int fd);
int RM_InsertRecord(int fd, const void *data, size_t length, RM_RID *rid);
int RM_GetRecord(int fd, RM_RID rid, void *out, size_t capacity, size_t *length);
int RM_DeleteRecord(int fd, RM_RID rid);
int RM_OpenScan(int fd, RM_Scan *scan);
int RM_GetNextRecord(RM_Scan *scan, RM_RID *rid, void *out, size_t capacity, size_t *length);
int RM_CloseScan(RM_Scan *scan);
int RM_GetSpace(int fd, RM_Space *space);
void RM_PrintError(const char *message);
#endif
