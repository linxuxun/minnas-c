#include "unity.h"
#include "cas.h"
#include "backend.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char tmpdir[256];

TEST_GROUP(cas) {
    Backend *be = backend_local_create(tmpdir);
    CAS *cas = cas_create(be);

    const char *data = "Hello MiniNAS!";
    uint8_t *d = (uint8_t *)data;
    size_t dlen = strlen(data);

    // Hash should be consistent
    uint8_t dig1[32], dig2[32];
    sha256_hash(d, dlen, dig1);
    sha256_hash(d, dlen, dig2);
    test(memcmp(dig1, dig2, 32) == 0);
    PASS();

    // Store and load
    bool ok = cas_write(cas, "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234", d, dlen);
    test(ok == true);
    PASS();

    uint8_t *out = NULL; size_t olen = 0;
    ok = cas_read(cas, "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234", &out, &olen);
    test(ok == true);
    test(olen == dlen);
    test(memcmp(out, d, dlen) == 0);
    PASS();

    free(out);
    cas_free(cas);
    backend_free(be);
    printf("  All CAS tests passed.\n");
}

int main() {
    extern int tests_passed, tests_failed;
    tests_passed = tests_failed = 0;

    snprintf(tmpdir, sizeof(tmpdir), "/tmp/minnas_test_XXXXXX");
    mkdtemp(tmpdir);

    printf(C_BLD "\n=== CAS Tests ===" C_RST "\n");
    test_group_cas();

    printf("\n" C_BLD "Results: ");
    if (tests_failed == 0) printf(C_GRN);
    else printf(C_RED);
    printf("%d/%d passed" C_RST "\n", tests_passed, tests_passed + tests_failed);

    // Cleanup
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    return tests_failed > 0 ? 1 : 0;
}
