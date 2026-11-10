#ifndef TOYDB_RM_INTERNAL_H
#define TOYDB_RM_INTERNAL_H
#include <string.h>
#include "rm.h"
/* Typed declarations for the legacy PF functions used by this layer. */
extern int PF_CreateFile(char *name);
extern int PF_CloseFile(int fd);
extern int PF_AllocPage(int fd, int *number, char **page);
extern int PF_GetThisPage(int fd, int number, char **page);
extern int PF_GetNextPage(int fd, int *number, char **page);
extern int PF_UnfixPage(int fd, int number, int dirty);

/* Explicit little-endian 16-bit fields; no struct padding or aligned casts. */
unsigned rm_u16(const char *p);
void rm_put16(char *p, unsigned value);
int rm_error(int error);
int rm_pf(int result);
int rm_release(int fd, int page, int dirty, int result);
int rm_open(char *name, int policy, const char *magic);
int rm_scan_open(int fd, RM_Scan *scan);
#endif
