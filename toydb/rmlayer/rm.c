#include "internal.h"

/* Header: magic[4], slot count:u16, data boundary:u16.
   Slot: offset:u16, length:u16, state:u16 (0 deleted, 1 live). */
static char *slot_at(char *page, unsigned slot)
{
    return page + RM_HEADER_SIZE + slot * RM_SLOT_SIZE;
}
static int valid_page(char *page)
{
    unsigned count = rm_u16(page + 4), boundary = rm_u16(page + 6);
    unsigned i, offset, length, state, bytes = 0;
    char *slot;
    if (memcmp(page, "RMP1", 4) || count > (PF_PAGE_SIZE - RM_HEADER_SIZE) / RM_SLOT_SIZE ||
        boundary < RM_HEADER_SIZE + count * RM_SLOT_SIZE || boundary > PF_PAGE_SIZE)
        return RME_CORRUPT;
    for (i = 0; i < count; i++) {
        slot = slot_at(page, i);
        offset = rm_u16(slot); length = rm_u16(slot + 2); state = rm_u16(slot + 4);
        if (state > 1) return RME_CORRUPT;
        if (state) {
            if (!length || offset < boundary || offset + length > PF_PAGE_SIZE) return RME_CORRUPT;
            bytes += length;
        } else if (offset || length) return RME_CORRUPT;
    }
    return bytes == PF_PAGE_SIZE - boundary ? RME_OK : RME_CORRUPT;
}
int RM_OpenFile(char *name, int policy) { return rm_open(name, policy, "RMP1"); }

/* Return FALSE without changing the page if it cannot fit the record. */
static int insert_page(char *page, const void *data, size_t length, int *number)
{
    unsigned count = rm_u16(page + 4), boundary = rm_u16(page + 6), slot;
    unsigned directory;
    char *entry;
    for (slot = 0; slot < count; slot++)
        if (!rm_u16(slot_at(page, slot) + 4)) break;
    directory = RM_HEADER_SIZE + (count + (slot == count)) * RM_SLOT_SIZE;
    if (directory > boundary || length > boundary - directory) return FALSE;
    boundary -= (unsigned)length;
    memcpy(page + boundary, data, length);
    entry = slot_at(page, slot);
    rm_put16(entry, boundary); rm_put16(entry + 2, (unsigned)length); rm_put16(entry + 4, 1);
    rm_put16(page + 4, count + (slot == count)); rm_put16(page + 6, boundary);
    *number = (int)slot;
    return TRUE;
}
int RM_InsertRecord(int fd, const void *data, size_t length, RM_RID *rid)
{
    int number = -1, slot, error, inserted;
    char *page;
    if (!data || !rid || !length) return rm_error(RME_INVALID);
    if (length > RM_MAX_RECORD) return rm_error(RME_TOOLARGE);
    while ((error = PF_GetNextPage(fd, &number, &page)) == PFE_OK) {
        error = valid_page(page);
        if (error != RME_OK) return rm_release(fd, number, FALSE, error);
        inserted = insert_page(page, data, length, &slot);
        error = rm_release(fd, number, inserted, RME_OK);
        if (error != RME_OK) return error;
        if (inserted) { rid->page = number; rid->slot = slot; return RME_OK; }
    }
    if (error != PFE_EOF) return rm_pf(error);
    error = PF_AllocPage(fd, &number, &page);
    if (error != PFE_OK) return rm_pf(error);
    memset(page, 0, PF_PAGE_SIZE);
    memcpy(page, "RMP1", 4); rm_put16(page + 6, PF_PAGE_SIZE);
    insert_page(page, data, length, &slot); /* Empty-page fit checked before allocation. */
    error = rm_release(fd, number, TRUE, RME_OK);
    if (error == RME_OK) { rid->page = number; rid->slot = slot; }
    return error;
}
static int get_slot(int fd, RM_RID rid, char **page, char **slot)
{
    int error;
    if (rid.page < 0 || rid.slot < 0) return rm_error(RME_NOTFOUND);
    error = PF_GetThisPage(fd, rid.page, page);
    if (error == PFE_INVALIDPAGE) return rm_error(RME_NOTFOUND);
    if (error != PFE_OK) return rm_pf(error);
    error = valid_page(*page);
    if (error != RME_OK) return rm_release(fd, rid.page, FALSE, error);
    if ((unsigned)rid.slot >= rm_u16(*page + 4))
        return rm_release(fd, rid.page, FALSE, RME_NOTFOUND);
    *slot = slot_at(*page, rid.slot);
    if (!rm_u16(*slot + 4)) return rm_release(fd, rid.page, FALSE, RME_NOTFOUND);
    return RME_OK;
}
int RM_GetRecord(int fd, RM_RID rid, void *out, size_t capacity, size_t *length)
{
    char *page, *slot;
    int error;
    if (!length || (!out && capacity)) return rm_error(RME_INVALID);
    error = get_slot(fd, rid, &page, &slot);
    if (error != RME_OK) return error;
    *length = rm_u16(slot + 2);
    if (capacity < *length) return rm_release(fd, rid.page, FALSE, RME_TOOSMALL);
    memcpy(out, page + rm_u16(slot), *length);
    return rm_release(fd, rid.page, FALSE, RME_OK);
}
int RM_DeleteRecord(int fd, RM_RID rid)
{
    char *page, *slot, packed[PF_PAGE_SIZE], *entry;
    unsigned count, i, boundary = PF_PAGE_SIZE, length;
    int error = get_slot(fd, rid, &page, &slot);
    if (error != RME_OK) return error;
    count = rm_u16(page + 4);
    memset(packed, 0, sizeof(packed));
    memcpy(packed, page, RM_HEADER_SIZE + count * RM_SLOT_SIZE);
    memset(slot_at(packed, rid.slot), 0, RM_SLOT_SIZE);
    for (i = 0; i < count; i++) {
        entry = slot_at(packed, i);
        if (!rm_u16(entry + 4)) continue;
        length = rm_u16(entry + 2);
        boundary -= length;
        memcpy(packed + boundary, page + rm_u16(entry), length);
        rm_put16(entry, boundary);
    }
    rm_put16(packed + 6, boundary);
    memcpy(page, packed, sizeof(packed));
    return rm_release(fd, rid.page, TRUE, RME_OK);
}
int RM_GetNextRecord(RM_Scan *scan, RM_RID *rid, void *out, size_t capacity, size_t *length)
{
    int number, error;
    unsigned i, count;
    char *page, *slot;
    if (!scan || !scan->active) return rm_error(RME_SCAN);
    if (!rid || !length || (!out && capacity)) return rm_error(RME_INVALID);
    if (scan->ended) return rm_error(RME_EOF);
    number = scan->page;
    error = number < 0 ? PF_GetNextPage(scan->fd, &number, &page) :
                        PF_GetThisPage(scan->fd, number, &page);
    for (;;) {
        if (error == PFE_EOF) { scan->ended = TRUE; return rm_error(RME_EOF); }
        if (error != PFE_OK) return rm_pf(error);
        error = valid_page(page);
        if (error != RME_OK) return rm_release(scan->fd, number, FALSE, error);
        scan->page = number;
        count = rm_u16(page + 4);
        for (i = scan->slot; i < count; i++) {
            slot = slot_at(page, i);
            if (!rm_u16(slot + 4)) continue;
            *length = rm_u16(slot + 2);
            if (capacity < *length) {
                scan->slot = i; /* Retry returns the same record. */
                return rm_release(scan->fd, number, FALSE, RME_TOOSMALL);
            }
            memcpy(out, page + rm_u16(slot), *length);
            error = rm_release(scan->fd, number, FALSE, RME_OK);
            if (error != RME_OK) return error;
            rid->page = number; rid->slot = i; scan->slot = i + 1;
            return RME_OK;
        }
        error = rm_release(scan->fd, number, FALSE, RME_OK);
        if (error != RME_OK) return error;
        error = PF_GetNextPage(scan->fd, &number, &page);
        if (error == PFE_OK) scan->slot = 0;
    }
}
int RM_GetSpace(int fd, RM_Space *space)
{
    int number = -1, error;
    unsigned i, count, payload;
    char *page, *slot;
    RM_Space result = {0};
    if (!space) return rm_error(RME_INVALID);
    while ((error = PF_GetNextPage(fd, &number, &page)) == PFE_OK) {
        error = valid_page(page);
        if (error != RME_OK) return rm_release(fd, number, FALSE, error);
        count = rm_u16(page + 4); payload = 0;
        for (i = 0; i < count; i++) {
            slot = slot_at(page, i);
            if (rm_u16(slot + 4)) { result.records++; payload += rm_u16(slot + 2); }
        }
        result.pages++; result.payload += payload;
        result.metadata += RM_HEADER_SIZE + count * RM_SLOT_SIZE;
        result.unused += PF_PAGE_SIZE - RM_HEADER_SIZE - count * RM_SLOT_SIZE - payload;
        error = rm_release(fd, number, FALSE, RME_OK);
        if (error != RME_OK) return error;
    }
    if (error != PFE_EOF) return rm_pf(error);
    *space = result;
    return RME_OK;
}
