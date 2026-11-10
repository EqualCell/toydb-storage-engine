#include <stdio.h>
#include "internal.h"

unsigned rm_u16(const char *p)
{
    const unsigned char *b = (const unsigned char *)p;
    return b[0] | ((unsigned)b[1] << 8);
}
void rm_put16(char *p, unsigned value)
{
    p[0] = (char)(value & 255);
    p[1] = (char)(value >> 8);
}

int RMerrno = RME_OK;
int rm_error(int error) { RMerrno = error; return error; }
int rm_pf(int result) { return result < 0 ? rm_error(RME_PF) : result; }

/* Always try to release our pin, including after validation/dirty errors. */
int rm_release(int fd, int page, int dirty, int result)
{
    int marked = PFE_OK, released;
    if (dirty) marked = PF_MarkDirty(fd, page);
    released = PF_UnfixPage(fd, page, dirty);
    if (marked != PFE_OK || released != PFE_OK) return rm_error(RME_PF);
    return result == RME_OK ? RME_OK : rm_error(result);
}

int RM_CreateFile(char *name)
{
    if (!name) return rm_error(RME_INVALID);
    return rm_pf(PF_CreateFile(name));
}
int RM_CloseFile(int fd) { return rm_pf(PF_CloseFile(fd)); }

int rm_open(char *name, int policy, const char *magic)
{
    int fd, number = -1, error, saved;
    char *page;
    if (!name) return rm_error(RME_INVALID);
    fd = PF_OpenFileWithPolicy(name, policy);
    if (fd < 0) return rm_pf(fd);
    error = PF_GetNextPage(fd, &number, &page);
    if (error == PFE_OK) {
        error = memcmp(page, magic, 4) == 0 ? RME_OK : RME_CORRUPT;
        error = rm_release(fd, number, FALSE, error);
    } else if (error == PFE_EOF) error = RME_OK;
    else error = RME_PF;
    if (error != RME_OK) {
        saved = PFerrno;
        PF_CloseFile(fd);
        PFerrno = saved;
        return rm_error(error);
    }
    return fd;
}

int rm_scan_open(int fd, RM_Scan *scan)
{
    int number = -1, result;
    char *page;
    if (!scan) return rm_error(RME_INVALID);
    /* Validate fd even for an empty file. No pin is retained by the cursor. */
    result = PF_GetNextPage(fd, &number, &page);
    if (result == PFE_OK) {
        result = rm_release(fd, number, FALSE, RME_OK);
        if (result != RME_OK) return result;
    } else if (result != PFE_EOF) return rm_pf(result);
    scan->fd = fd; scan->page = -1; scan->slot = 0;
    scan->active = TRUE; scan->ended = FALSE;
    return RME_OK;
}
int RM_OpenScan(int fd, RM_Scan *scan) { return rm_scan_open(fd, scan); }
int RM_CloseScan(RM_Scan *scan)
{
    if (!scan || !scan->active) return rm_error(RME_SCAN);
    scan->active = FALSE;
    return RME_OK;
}
void RM_PrintError(const char *message)
{
    const char *text;
    switch (RMerrno) {
    case RME_OK: text = "no error"; break;
    case RME_PF: PF_PrintError((char *)message); return;
    case RME_INVALID: text = "invalid argument or zero-length record"; break;
    case RME_TOOLARGE: text = "record exceeds format capacity"; break;
    case RME_NOTFOUND: text = "invalid or deleted RID"; break;
    case RME_TOOSMALL: text = "output buffer too small"; break;
    case RME_EOF: text = "end of scan"; break;
    case RME_CORRUPT: text = "wrong format or corrupt page"; break;
    default: text = "invalid scan cursor"; break;
    }
    fprintf(stderr, "%s: %s\n", message, text);
}
