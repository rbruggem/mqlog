#ifndef LOG_TESTFW_H
#define LOG_TESTFW_H

#include <stdio.h>

#define NEUTRAL "\x1B[0m"
#define GREEN   "\x1B[32m"
#define RED     "\x1B[31m"

void testfw_add(void (*)(int*), const char*);
int testfw_run();

#define TEST(name) void name(int*); void __attribute__((constructor)) pre_##name() {testfw_add(name, #name);} void name(int* __errors __attribute__((unused)))

#define FAILED(name) fprintf(stderr, "[ %sFAILED%s ] %s (%s:%d)\n", RED, NEUTRAL, name, __FILE__, __LINE__)
#define PASSED(name) printf("[ %sPASSED%s ] %s\n", GREEN, NEUTRAL, name);

#define ASSERT(stmt) if (!(stmt)) {++(*__errors); FAILED(__FUNCTION__);}

#endif
