/* GNU ld wraps write only in this test executable; production has no test hook. */
#include <unistd.h>
#include <errno.h>
#include "testutil.h"

static int fail_write;
static int fail_read;
extern ssize_t __real_read(int fd, void *buf, size_t count);

ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    if (fail_read && count == sizeof(PFfpage))
        return 0; /* Simulate a truncated data page. */
    return __real_read(fd, buf, count);
}

extern ssize_t __real_write(int fd, const void *buf, size_t count);

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if (fail_write && count == sizeof(PFfpage)) {
        errno = EIO;
        return -1;
    }
    return __real_write(fd, buf, count);
}

int main(void)
{
    int fd, value = 456;
    char *page;
    fixture("failure.pf", 2, 0);
    PF_Init();
    CHECK(PF_SetBufferSize(1) == PFE_OK);
    fd = PF_OpenFile("failure.pf");
    CHECK(fd >= 0);
    fail_read = TRUE;
    expect_error(PF_GetThisPage(fd, 0, &page), PFE_INCOMPLETEREAD);
    expect_stats(1, 0, 0, 0, 0);
    fail_read = FALSE;
    CHECK(PF_GetThisPage(fd, 0, &page) == PFE_OK);
    expect_stats(2, 0, 1, 0, 0);
    memcpy(page, &value, sizeof(value));
    CHECK(PF_MarkDirty(fd, 0) == PFE_OK);
    CHECK(PF_UnfixPage(fd, 0, FALSE) == PFE_OK);
    PF_ResetStats();
    fail_write = TRUE;
    expect_error(PF_GetThisPage(fd, 1, &page), PFE_UNIX);
    expect_stats(1, 0, 0, 0, 0);
    CHECK(PF_GetThisPage(fd, 0, &page) == PFE_OK);
    CHECK(memcmp(page, &value, sizeof(value)) == 0);
    CHECK(PF_UnfixPage(fd, 0, FALSE) == PFE_OK);
    expect_error(PF_CloseFile(fd), PFE_UNIX);
    expect_stats(2, 1, 0, 0, 0);
    CHECK(disk_value("failure.pf", 0) == 0);
    fail_write = FALSE;
    access_page(fd, 1); /* Retry must still write dirty A. */
    expect_stats(3, 1, 1, 1, 0);
    CHECK(disk_value("failure.pf", 0) == value);
    CHECK(PF_GetThisPage(fd, 1, &page) == PFE_OK);
    memcpy(page, &value, sizeof(value));
    CHECK(PF_UnfixPage(fd, 1, TRUE) == PFE_OK);
    fail_write = TRUE;
    expect_error(PF_CloseFile(fd), PFE_UNIX);
    fail_write = FALSE;
    CHECK(PF_CloseFile(fd) == PFE_OK);
    expect_stats(4, 2, 1, 2, 0);
    CHECK(disk_value("failure.pf", 1) == value);
    puts("PASS: failed eviction/close preserve dirty pages and successful-I/O counts");
    return 0;
}
