/* test_util.h - minimal test harness: CHECK(cond) records failures; TEST_DONE() returns exit code. */
#ifndef WP_TEST_UTIL_H
#define WP_TEST_UTIL_H
#include <stdio.h>
static int wp_test_failures = 0, wp_test_checks = 0;
#define CHECK(cond) do { wp_test_checks++; if (!(cond)) { wp_test_failures++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECKF(cond, ...) do { wp_test_checks++; if (!(cond)) { wp_test_failures++; \
    fprintf(stderr, "FAIL %s:%d: %s :: ", __FILE__, __LINE__, #cond); fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); } } while (0)
#define TEST_DONE() (printf("%s: %d checks, %d failures\n", __FILE__, wp_test_checks, wp_test_failures), \
    wp_test_failures ? 1 : 0)
#endif
