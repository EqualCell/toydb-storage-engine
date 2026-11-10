/* Shared test fixtures use stdio, never the buffer pool. */
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

static void fixture(char *name, int pages, int base)
{
    FILE *file = fopen(name, "wb");
    PFhdr_str header;
    PFfpage page;
    int i, value;
    CHECK(file != NULL);
    header.firstfree = PF_PAGE_LIST_END;
    header.numpages = pages;
    CHECK(fwrite(&header, sizeof(header), 1, file) == 1);
    memset(&page, 0, sizeof(page));
    page.nextfree = PF_PAGE_USED;
    for (i = 0; i < pages; i++) {
        value = base + i;
        memcpy(page.pagebuf, &value, sizeof(value));
        CHECK(fwrite(&page, sizeof(page), 1, file) == 1);
    }
    CHECK(fclose(file) == 0);
}

static int disk_value(char *name, int number)
{
    FILE *file = fopen(name, "rb");
    PFfpage page;
    int value;
    CHECK(file != NULL);
    CHECK(fseek(file, (long)(PF_HDR_SIZE + number * sizeof(page)), SEEK_SET) == 0);
    CHECK(fread(&page, sizeof(page), 1, file) == 1);
    memcpy(&value, page.pagebuf, sizeof(value));
    CHECK(fclose(file) == 0);
    return value;
}

static void access_page(int fd, int number)
{
    char *page;
    CHECK(PF_GetThisPage(fd, number, &page) == PFE_OK);
    CHECK(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
}

static void expect_stats(unsigned long long requests, unsigned long long hits,
                         unsigned long long reads, unsigned long long writes,
                         unsigned long long allocations)
{
    PF_Stats stats = PF_GetStats();
    CHECK(stats.logical_requests == requests);
    CHECK(stats.cache_hits == hits);
    CHECK(stats.cache_hits + stats.cache_misses == requests);
    CHECK(stats.physical_reads == reads);
    CHECK(stats.physical_writes == writes);
    CHECK(stats.page_allocations == allocations);
}
