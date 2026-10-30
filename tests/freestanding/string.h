#ifndef TEST_FREESTANDING_STRING_H
#define TEST_FREESTANDING_STRING_H
#include <stddef.h>
extern "C" {
void* memcpy(void*, const void*, size_t);
void* memset(void*, int, size_t);
int memcmp(const void*, const void*, size_t);
size_t strlen(const char*);
char* strncpy(char*, const char*, size_t);
}
#endif
