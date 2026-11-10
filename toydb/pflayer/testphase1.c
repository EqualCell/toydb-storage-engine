#include "testutil.h"

static int open_fixture(int policy)
{
    int fd;
    fixture("pages.pf", 4, 100);
    fd = policy == -1 ? PF_OpenFile("pages.pf") :
         PF_OpenFileWithPolicy("pages.pf", policy);
    CHECK(fd >= 0);
    return fd;
}

static void replacement(int policy, int allocate)
{
    int fd = open_fixture(policy), number, survivor, victim;
    char *page;
    if (allocate == 2) {
        CHECK(PF_DisposePage(fd, 3) == PFE_OK);
        CHECK(PF_CloseFile(fd) == PFE_OK);
        fd = PF_OpenFileWithPolicy("pages.pf", policy);
        CHECK(fd >= 0);
    }
    access_page(fd, 0);
    access_page(fd, 1);
    access_page(fd, 0);
    if (allocate) {
        PF_ResetStats();
        CHECK(PF_AllocPage(fd, &number, &page) == PFE_OK);
        CHECK(number == (allocate == 2 ? 3 : 4));
        expect_stats(allocate == 2 ? 1 : 0, 0, allocate == 2 ? 1 : 0, 0, 1);
        CHECK(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
    } else
        access_page(fd, 2);
    survivor = policy == PF_MRU ? 1 : 0;
    victim = 1 - survivor;
    PF_ResetStats();
    access_page(fd, survivor);
    expect_stats(1, 1, 0, 0, 0);
    access_page(fd, victim);
    expect_stats(2, 1, 1, allocate && policy != PF_MRU ? 1 : 0, 0);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    if (allocate == 2) {
        fd = PF_OpenFile("pages.pf");
        CHECK(fd >= 0);
        access_page(fd, 3); /* Reused page remains allocated on disk. */
        CHECK(PF_CloseFile(fd) == PFE_OK);
    }
}

static void fixed_pages(int policy)
{
    int fd = open_fixture(policy), released;
    char *page;
    CHECK(PF_GetThisPage(fd, 0, &page) == PFE_OK);
    CHECK(PF_GetThisPage(fd, 1, &page) == PFE_OK);
    PF_ResetStats();
    expect_error(PF_GetThisPage(fd, 2, &page), PFE_NOBUF);
    expect_error(PF_GetThisPage(fd, 0, &page), PFE_PAGEFIXED);
    expect_error(PF_GetThisPage(fd, 99, &page), PFE_INVALIDPAGE);
    expect_error(PF_GetThisPage(-1, 0, &page), PFE_FD);
    expect_stats(2, 1, 0, 0, 0);
    released = policy == PF_MRU ? 0 : 1;
    CHECK(PF_UnfixPage(fd, released, FALSE) == PFE_OK);
    /* MRU: put the still-fixed page at the head; LRU: it is already the tail. */
    if (policy == PF_MRU)
        CHECK(PF_MarkDirty(fd, 1) == PFE_OK);
    CHECK(PF_GetThisPage(fd, 2, &page) == PFE_OK);
    CHECK(PF_UnfixPage(fd, 2, FALSE) == PFE_OK);
    CHECK(PF_UnfixPage(fd, 1 - released, FALSE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
}

static void dirty_pages(int policy)
{
    int fd = open_fixture(policy), value = 987;
    char *page;
    expect_error(PF_MarkDirty(-1, 0), PFE_FD);
    expect_error(PF_MarkDirty(fd, 4), PFE_INVALIDPAGE);
    expect_error(PF_MarkDirty(fd, 0), PFE_PAGENOTINBUF);
    CHECK(PF_GetThisPage(fd, 0, &page) == PFE_OK);
    memcpy(page, &value, sizeof(value));
    CHECK(PF_MarkDirty(fd, 0) == PFE_OK);
    CHECK(PF_UnfixPage(fd, 0, FALSE) == PFE_OK);
    expect_error(PF_MarkDirty(fd, 0), PFE_PAGEUNFIXED);
    CHECK(PF_GetThisPage(fd, 1, &page) == PFE_OK);
    PF_ResetStats();
    access_page(fd, 2); /* B stays fixed: A must be evicted under either policy. */
    expect_stats(1, 0, 1, 1, 0);
    CHECK(disk_value("pages.pf", 0) == value);
    PF_ResetStats();
    access_page(fd, 3); /* Evict clean C. */
    expect_stats(1, 0, 1, 0, 0);
    memcpy(page, &value, sizeof(value)); /* page still points to fixed B */
    CHECK(PF_UnfixPage(fd, 1, TRUE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    expect_stats(1, 0, 1, 1, 0);
    fd = PF_OpenFile("pages.pf");
    CHECK(fd >= 0);
    CHECK(PF_GetThisPage(fd, 0, &page) == PFE_OK);
    CHECK(memcmp(page, &value, sizeof(value)) == 0);
    CHECK(PF_UnfixPage(fd, 0, FALSE) == PFE_OK);
    CHECK(PF_GetThisPage(fd, 1, &page) == PFE_OK);
    CHECK(memcmp(page, &value, sizeof(value)) == 0);
    CHECK(PF_UnfixPage(fd, 1, FALSE) == PFE_OK);
    CHECK(PF_CloseFile(fd) == PFE_OK);
}

static void mixed_files(int policy)
{
    int a, b, value = 999, original = 200;
    char *page;
    fixture("a.pf", 3, 100);
    fixture("b.pf", 3, 200);
    a = PF_OpenFileWithPolicy("a.pf", policy == PF_MRU ? PF_LRU : PF_MRU);
    b = PF_OpenFileWithPolicy("b.pf", policy);
    CHECK(a >= 0 && b >= 0);
    CHECK(PF_GetThisPage(a, 0, &page) == PFE_OK);
    memcpy(page, &value, sizeof(value));
    CHECK(PF_MarkDirty(a, 0) == PFE_OK);
    CHECK(PF_UnfixPage(a, 0, FALSE) == PFE_OK);
    CHECK(PF_GetThisPage(b, 0, &page) == PFE_OK);
    CHECK(memcmp(page, &original, sizeof(int)) == 0);
    CHECK(PF_UnfixPage(b, 0, FALSE) == PFE_OK);
    if (policy == PF_MRU)
        access_page(a, 0); /* A is head for MRU, tail for LRU. */
    PF_ResetStats();
    access_page(b, 1);
    expect_stats(1, 0, 1, 1, 0);
    CHECK(disk_value("a.pf", 0) == value);
    CHECK(disk_value("b.pf", 0) == 200);
    access_page(b, 0);
    expect_stats(2, 1, 1, 1, 0);
    access_page(a, 0);
    expect_stats(3, 1, 2, 1, 0);
    CHECK(PF_CloseFile(a) == PFE_OK);
    CHECK(PF_CloseFile(b) == PFE_OK);
}

static void stats_and_reset(void)
{
    int fd = open_fixture(PF_LRU), number;
    char *page;
    PF_ResetStats();
    access_page(fd, 0);
    access_page(fd, 0);
    access_page(fd, 1);
    expect_stats(3, 1, 2, 0, 0);
    PF_ResetStats();
    expect_stats(0, 0, 0, 0, 0);
    access_page(fd, 2); /* Reset must not reorder: A is still the LRU victim. */
    access_page(fd, 1);
    expect_stats(2, 1, 1, 0, 0);
    access_page(fd, 0);
    expect_stats(3, 1, 2, 0, 0);
    CHECK(PF_CloseFile(fd) == PFE_OK);
    fd = PF_OpenFile("pages.pf");
    CHECK(fd >= 0);
    PF_ResetStats();
    number = -1;
    CHECK(PF_GetFirstPage(fd, &number, &page) == PFE_OK);
    CHECK(number == 0);
    CHECK(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
    expect_stats(1, 0, 1, 0, 0); /* No duplicate counting through GetNextPage. */
    CHECK(PF_AllocPage(fd, &number, &page) == PFE_OK);
    CHECK(PF_UnfixPage(fd, number, FALSE) == PFE_OK);
    expect_stats(1, 0, 1, 0, 1); /* Appending is not a disk read. */
    CHECK(PF_CloseFile(fd) == PFE_OK);
    expect_stats(1, 0, 1, 1, 1);
}

static void invalid_policy(void)
{
    int i, fd[PF_FTAB_SIZE];
    fixture("pages.pf", 1, 0);
    for (i = 0; i < PF_FTAB_SIZE + 2; i++)
        expect_error(PF_OpenFileWithPolicy("missing.pf", -8), PFE_INVALIDPOLICY);
    PF_PrintError("expected invalid policy");
    for (i = 0; i < PF_FTAB_SIZE; i++) {
        fd[i] = PF_OpenFile("pages.pf");
        CHECK(fd[i] >= 0);
    }
    expect_error(PF_OpenFile("pages.pf"), PFE_FTABFULL);
    for (i = 0; i < PF_FTAB_SIZE; i++)
        CHECK(PF_CloseFile(fd[i]) == PFE_OK);
}

int main(int argc, char **argv)
{
    int policy;
    CHECK(argc == 3);
    policy = atoi(argv[2]);
    PF_Init();
    CHECK(PF_SetBufferSize(2) == PFE_OK);
    if (strcmp(argv[1], "replacement") == 0) replacement(policy, FALSE);
    else if (strcmp(argv[1], "allocation") == 0) replacement(policy, TRUE);
    else if (strcmp(argv[1], "reuse") == 0) replacement(policy, 2);
    else if (strcmp(argv[1], "fixed") == 0) fixed_pages(policy);
    else if (strcmp(argv[1], "dirty") == 0) dirty_pages(policy);
    else if (strcmp(argv[1], "mixed") == 0) mixed_files(policy);
    else if (strcmp(argv[1], "stats") == 0) stats_and_reset();
    else if (strcmp(argv[1], "invalid") == 0) invalid_policy();
    else CHECK(FALSE);
    printf("PASS: %s policy=%d\n", argv[1], policy);
    return 0;
}
