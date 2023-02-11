#ifndef IMPORTS_H
#define IMPORTS_H

#include "types.h"
#include <stdarg.h>
#include <stdlib.h>

#define MCP_SVC_BASE ((void *) 0x050567EC)

typedef enum {
    MEM_PERM_R  = 1 << 0,
    MEM_PERM_W  = 1 << 1,
    MEM_PERM_RW = (MEM_PERM_R | MEM_PERM_W),
} MemPermFlags;

int IOS_JoinThread(int threadid, int32_t *returned_value);

int IOS_CreateMessageQueue(uint32_t *ptr, uint32_t n_msgs);
int IOS_DestroyMessageQueue(int queueid);
int IOS_SendMessage(int queueid, uint32_t message, uint32_t flags);
int IOS_ReceiveMessage(int queueid, uint32_t *message, uint32_t flags);

int IOS_CreateTimer(uint32_t time_us, uint32_t repeat_time_us, int queueid, uint32_t message);
int IOS_RestartTimer(int timerid, uint32_t time_us, uint32_t repeat_time_us);
int IOS_DestroyTimer(int timerid);

void IOS_InvalidateDCache(void *ptr, uint32_t len);
void IOS_FlushDCache(void *ptr, uint32_t len);

int32_t IOS_CheckIOPAddressRange(void *ptr, uint32_t size, MemPermFlags perm);
uint32_t IOS_VirtToPhys(void *ptr);

int IOS_CreateSemaphore(int32_t maxCount, int32_t initialCount);
int IOS_WaitSemaphore(int id, uint32_t tryWait);
int IOS_SignalSemaphore(int id);
int IOS_DestroySemaphore(int id);

#endif
