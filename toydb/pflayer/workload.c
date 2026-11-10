/* One process, one cold-cache measured workload, followed by value validation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"

#define REQUIRE(test) do { if (!(test)) { \
    fprintf(stderr, "workload line %d: %s (PFerrno=%d)\n", \
            __LINE__, #test, PFerrno); exit(1); } } while (0)

int main(int argc, char **argv)
{
    int capacity, policy, pages, operations, percent, fd, i, number, update, value;
    int *expected, *page_sequence, *write_sequence;
    unsigned long long reads = 0, writes = 0, checksum = 0;
    char *page;
    FILE *trace;
    PF_Stats stats;

    REQUIRE(argc == 9);
    capacity = atoi(argv[1]);
    policy = strcmp(argv[2], "LRU") == 0 ? PF_LRU : PF_MRU;
    REQUIRE(strcmp(argv[2], "LRU") == 0 || strcmp(argv[2], "MRU") == 0);
    pages = atoi(argv[3]);
    operations = atoi(argv[4]);
    percent = atoi(argv[7]);
    REQUIRE(capacity > 0 && pages > capacity && operations > 0);
    REQUIRE(percent >= 0 && percent <= 100);
    expected = calloc(pages, sizeof(int));
    page_sequence = malloc(operations * sizeof(int));
    write_sequence = malloc(operations * sizeof(int));
    REQUIRE(expected != NULL && page_sequence != NULL && write_sequence != NULL);
    trace = fopen(argv[8], "r");
    REQUIRE(trace != NULL);
    for (i = 0; i < operations; i++) {
        REQUIRE(fscanf(trace, "%d %d", &number, &update) == 2);
        REQUIRE(number >= 0 && number < pages && (update == 0 || update == 1));
        page_sequence[i] = number;
        write_sequence[i] = update;
    }
    REQUIRE(fscanf(trace, "%d", &number) == EOF);
    REQUIRE(fclose(trace) == 0);

    PF_Init();
    REQUIRE(PF_SetBufferSize(capacity) == PFE_OK);
    REQUIRE(PF_CreateFile("workload.pf") == PFE_OK);
    fd = PF_OpenFileWithPolicy("workload.pf", policy);
    REQUIRE(fd >= 0);
    for (i = 0; i < pages; i++) {
        REQUIRE(PF_AllocPage(fd, &number, &page) == PFE_OK);
        REQUIRE(number == i);
        memset(page, 0, PF_PAGE_SIZE);
        expected[i] = i;
        memcpy(page, &expected[i], sizeof(int));
        REQUIRE(PF_UnfixPage(fd, i, TRUE) == PFE_OK);
    }
    REQUIRE(PF_CloseFile(fd) == PFE_OK);
    fd = PF_OpenFileWithPolicy("workload.pf", policy);
    REQUIRE(fd >= 0);
    PF_ResetStats();
    for (i = 0; i < operations; i++) {
        number = page_sequence[i];
        REQUIRE(PF_GetThisPage(fd, number, &page) == PFE_OK);
        memcpy(&value, page, sizeof(value));
        REQUIRE(value == expected[number]);
        if (write_sequence[i]) {
            value++;
            memcpy(page, &value, sizeof(value));
            REQUIRE(PF_MarkDirty(fd, number) == PFE_OK);
            REQUIRE(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
            expected[number] = value;
            writes++;
        } else {
            checksum += value;
            REQUIRE(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
            reads++;
        }
    }
    REQUIRE(PF_CloseFile(fd) == PFE_OK);
    stats = PF_GetStats(); /* Includes final flush; excludes validation below. */
    REQUIRE(reads + writes == (unsigned long long)operations);
    REQUIRE(writes == (unsigned long long)operations * percent / 100);
    REQUIRE(stats.logical_requests == reads + writes);
    REQUIRE(stats.cache_hits + stats.cache_misses == stats.logical_requests);
    REQUIRE(stats.physical_reads == stats.cache_misses);
    REQUIRE(stats.page_allocations == 0);

    fd = PF_OpenFile("workload.pf");
    REQUIRE(fd >= 0);
    for (i = 0; i < pages; i++) {
        REQUIRE(PF_GetThisPage(fd, i, &page) == PFE_OK);
        memcpy(&value, page, sizeof(value));
        REQUIRE(value == expected[i]);
        REQUIRE(PF_UnfixPage(fd, i, FALSE) == PFE_OK);
    }
    REQUIRE(PF_CloseFile(fd) == PFE_OK);
    REQUIRE(PF_DestroyFile("workload.pf") == PFE_OK);
    puts("capacity,policy,page_count,operation_count,seed,pattern,write_percent,completed_reads,completed_writes,logical_requests,cache_hits,cache_misses,physical_reads,physical_writes,page_allocations,read_checksum");
    printf("%d,%s,%d,%d,%s,%s,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
           capacity, argv[2], pages, operations, argv[5], argv[6], percent,
           reads, writes, stats.logical_requests, stats.cache_hits, stats.cache_misses,
           stats.physical_reads, stats.physical_writes, stats.page_allocations, checksum);
    free(expected);
    free(page_sequence);
    free(write_sequence);
    return 0;
}
