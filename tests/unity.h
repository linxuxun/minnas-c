#ifndef UNITY_H
#define UNITY_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int tests_passed;
extern int tests_failed;

#define TEST_GROUP(name) void test_group_##name(void)
#define RUN_TESTS(func) do {     printf("  " #func "... ");     fflush(stdout);     func(); } while(0)

#define test(expr) do {     if (!(expr)) {         printf(C_RED "FAIL" C_RST " at %s:%d\n", __FILE__, __LINE__);         tests_failed++;         return;     } } while(0)

#define test_str_eq(a, b) do {     if (strcmp(a, b) != 0) {         printf(C_RED "FAIL" C_RST " \"%s\" != \"%s\" at %s:%d\n", a, b, __FILE__, __LINE__);         tests_failed++; return;     } } while(0)

#define test_int_eq(a, b) do {     if ((a) != (b)) {         printf(C_RED "FAIL" C_RST " %d != %d at %s:%d\n", a, b, __FILE__, __LINE__);         tests_failed++; return;     } } while(0)

#define PASS() do { tests_passed++; printf(C_GRN "ok" C_RST "\n"); } while(0)

#define MAIN_WITH_TESTS(func) int main() {     extern int tests_passed; extern int tests_failed;     tests_passed = tests_failed = 0;     printf(C_BLD "\n=== " #func " ===" C_RST "\n");     func();     printf("\n" C_BLD "Results: ");     if (tests_failed == 0) printf(C_GRN);     else printf(C_RED);     printf("%d/%d passed" C_RST "\n", tests_passed, tests_passed + tests_failed);     return tests_failed > 0 ? 1 : 0; }

#endif
