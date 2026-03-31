#include "unity.h"
#include "vfs.h"
#include "cas.h"
#include "backend.h"
#include "snapshot.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char tmpdir[256];

TEST_GROUP(vfs) {
    Backend *be = backend_local_create(tmpdir);
    CAS *cas = cas_create(be);
    Tree t = {0};
    VFS *vfs = vfs_create(cas, &t);

    // Test write and read
    int fd = vfs_open(vfs, "/hello.txt", "w");
    test(fd >= 0);
    PASS();
    int n = vfs_write(vfs, fd, (uint8_t *)"Hello", 5);
    test(n == 5);
    PASS();
    vfs_close(vfs, fd);
    PASS();

    fd = vfs_open(vfs, "/hello.txt", "r");
    test(fd >= 0);
    uint8_t buf[64];
    int r = vfs_read(vfs, fd, buf, 64);
    test(r == 5);
    test(memcmp(buf, "Hello", 5) == 0);
    PASS();
    vfs_close(vfs, fd);
    PASS();

    // Test append
    fd = vfs_open(vfs, "/hello.txt", "a");
    vfs_write(vfs, fd, (uint8_t *)" World!", 6);
    vfs_close(vfs, fd);
    fd = vfs_open(vfs, "/hello.txt", "r");
    r = vfs_read(vfs, fd, buf, 64);
    test(r == 11);
    test(memcmp(buf, "Hello World!", 11) == 0);
    PASS();
    vfs_close(vfs, fd);

    // Test exists
    test(vfs_exists(vfs, "/hello.txt") == true);
    test(vfs_exists(vfs, "/nonexistent.txt") == false);
    PASS();

    // Test rm
    int ok = vfs_rm(vfs, "/hello.txt");
    test(ok == 0);
    test(vfs_exists(vfs, "/hello.txt") == false);
    PASS();

    // Test truncate
    fd = vfs_open(vfs, "/data.txt", "w");
    vfs_write(vfs, fd, (uint8_t *)"0123456789", 10);
    vfs_close(vfs, fd);
    vfs_truncate(vfs, "/data.txt", 5);
    fd = vfs_open(vfs, "/data.txt", "r");
    r = vfs_read(vfs, fd, buf, 64);
    test(r == 5);
    test(memcmp(buf, "01234", 5) == 0);
    PASS();
    vfs_close(vfs, fd);

    vfs_free(vfs);
    cas_free(cas);
    backend_free(be);
    printf("  All VFS tests passed.\n");
}

int main() {
    extern int tests_passed, tests_failed;
    tests_passed = tests_failed = 0;
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/minnas_test_XXXXXX");
    mkdtemp(tmpdir);
    printf(C_BLD "\n=== VFS Tests ===" C_RST "\n");
    test_group_vfs();
    printf("\n" C_BLD "Results: ");
    if (tests_failed == 0) printf(C_GRN);
    else printf(C_RED);
    printf("%d/%d passed" C_RST "\n", tests_passed, tests_passed + tests_failed);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
    return tests_failed > 0 ? 1 : 0;
}
