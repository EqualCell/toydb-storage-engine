/* Run each scenario in a fresh process: allocation locks configuration. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "pftypes.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "line %d: %s (PFerrno=%d)\n", \
            __LINE__, #test, PFerrno); exit(1); } } while (0)

static void expect_error(int result, int expected)
{
    CHECK(result == expected);
    CHECK(PFerrno == expected);
}

/* Write disk pages directly so fixture creation cannot lock the buffer pool. */
static void make_fixture(int pages)
{
    FILE *file;
    PFhdr_str header;
    PFfpage page;
    int i;

    file = fopen("capacity.pf", "wb");
    CHECK(file != NULL);
    header.firstfree = PF_PAGE_LIST_END;
    header.numpages = pages;
    CHECK(fwrite(&header, sizeof(header), 1, file) == 1);
    memset(&page, 0, sizeof(page));
    page.nextfree = PF_PAGE_USED;
    for (i = 0; i < pages; i++)
        CHECK(fwrite(&page, sizeof(page), 1, file) == 1);
    CHECK(fclose(file) == 0);
}

static void check_disk_value(void)
{
    FILE *file;
    PFfpage page;

    file = fopen("capacity.pf", "rb");
    CHECK(file != NULL);
    CHECK(fseek(file, (long)PF_HDR_SIZE, SEEK_SET) == 0);
    CHECK(fread(&page, sizeof(page), 1, file) == 1);
    CHECK(strcmp(page.pagebuf, "dirty page A") == 0);
    CHECK(fclose(file) == 0);
}

static void test_capacity(int capacity, int configure)
{
    int fd, i;
    char *page;

    make_fixture(capacity + 1);
    PF_Init();
    fd = PF_OpenFile("capacity.pf");
    CHECK(fd >= 0);
    if (configure) {
        CHECK(PF_SetBufferSize(capacity + 1) == PFE_OK);
        CHECK(PF_SetBufferSize(capacity) == PFE_OK);
        expect_error(PF_SetBufferSize(0), PFE_INVALIDBUFSIZE);
        PF_PrintError("expected invalid capacity");
        expect_error(PF_SetBufferSize(-3), PFE_INVALIDBUFSIZE);
    }
    for (i = 0; i < capacity; i++) {
        CHECK(PF_GetThisPage(fd, i, &page) == PFE_OK);
        if (i == 0) {
            strcpy(page, "dirty page A");
            if (configure) {
                expect_error(PF_SetBufferSize(capacity + 1), PFE_BUFALLOCATED);
                PF_PrintError("expected allocation lock");
            }
        }
    }
    expect_error(PF_GetThisPage(fd, capacity, &page), PFE_NOBUF);
    CHECK(PF_UnfixPage(fd, 0, TRUE) == PFE_OK);
    CHECK(PF_GetThisPage(fd, capacity, &page) == PFE_OK);
    /* All other frames are fixed, so A must have been evicted and written. */
    check_disk_value();
    for (i = 1; i <= capacity; i++)
        CHECK(PF_UnfixPage(fd, i, FALSE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    if (configure)
        expect_error(PF_SetBufferSize(capacity + 1), PFE_BUFALLOCATED);

    fd = PF_OpenFile("capacity.pf");
    CHECK(fd >= 0);
    for (i = 0; i < capacity; i++) {
        CHECK(PF_GetThisPage(fd, i, &page) == PFE_OK);
        if (i == 0)
            CHECK(strcmp(page, "dirty page A") == 0);
    }
    /* Rejected changes and reuse of free frames must preserve the limit. */
    expect_error(PF_GetThisPage(fd, capacity, &page), PFE_NOBUF);
    for (i = 0; i < capacity; i++)
        CHECK(PF_UnfixPage(fd, i, FALSE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    CHECK(PF_DestroyFile("capacity.pf") == PFE_OK);
}

static void test_allocation(void)
{
    int fd, number;
    char *page;

    PF_Init();
    expect_error(PF_SetBufferSize(0), PFE_INVALIDBUFSIZE);
    expect_error(PF_SetBufferSize(-1), PFE_INVALIDBUFSIZE);
    CHECK(PF_CreateFile("allocation.pf") == PFE_OK);
    fd = PF_OpenFile("allocation.pf");
    CHECK(fd >= 0);
    CHECK(PF_SetBufferSize(2) == PFE_OK);
    CHECK(PF_AllocPage(fd, &number, &page) == PFE_OK);
    CHECK(number == 0);
    expect_error(PF_SetBufferSize(3), PFE_BUFALLOCATED);
    CHECK(PF_AllocPage(fd, &number, &page) == PFE_OK);
    CHECK(number == 1);
    expect_error(PF_AllocPage(fd, &number, &page), PFE_NOBUF);
    CHECK(PF_UnfixPage(fd, 0, FALSE) == PFE_OK);
    CHECK(PF_AllocPage(fd, &number, &page) == PFE_OK);
    CHECK(number == 2);
    CHECK(PF_UnfixPage(fd, 1, FALSE) == PFE_OK);
    CHECK(PF_UnfixPage(fd, 2, FALSE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    expect_error(PF_SetBufferSize(3), PFE_BUFALLOCATED);
    CHECK(PF_DestroyFile("allocation.pf") == PFE_OK);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (strcmp(argv[1], "default") == 0)
        test_capacity(20, FALSE);
    else if (strcmp(argv[1], "two") == 0)
        test_capacity(2, TRUE);
    else if (strcmp(argv[1], "one") == 0)
        test_capacity(1, TRUE);
    else if (strcmp(argv[1], "large") == 0)
        test_capacity(25, TRUE);
    else if (strcmp(argv[1], "allocate") == 0)
        test_allocation();
    else
        CHECK(FALSE);
    printf("PASS: %s\n", argv[1]);
    return 0;
}
