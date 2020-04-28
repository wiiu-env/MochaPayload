#ifndef __UTILS_H_
#define __UTILS_H_

#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

//Need to have log_init() called beforehand.
void dumpHex(const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H_
