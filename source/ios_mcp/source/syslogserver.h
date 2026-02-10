#pragma once

#include "ipc_types.h"
#include <assert.h>
#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#ifndef CHECK_SIZE
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == size, #type " must be " #size " bytes")
#endif

typedef struct PACKED SyslogBufferHeader {
    uint32_t magic;
    uint32_t entries;
    uint32_t activeEntry;
    uint32_t checksum;
} SyslogBufferHeader;
CHECK_SIZE(SyslogBufferHeader, 0x10);

typedef struct PACKED SyslogCircularBuffer {
    uint32_t writeOffset;
    uint32_t readOffset;
    uint32_t capacity;
    void *dataPtr;
} SyslogCircularBuffer;
CHECK_SIZE(SyslogCircularBuffer, 0x10);

typedef struct PACKED SyslogBufferEntry {
    SyslogCircularBuffer buffer;
    char data[0x40000];
} SyslogBufferEntry;
CHECK_SIZE(SyslogBufferEntry, 0x40010);

typedef struct PACKED SyslogBuffers {
    SyslogBufferHeader header;
    SyslogBufferEntry entries[2];
} SyslogBuffers;
CHECK_SIZE(SyslogBuffers, 0x80030);

#define SYSLOG_BUFFER_FLAG_USB_SERIAL 0x08
#define SYSLOG_BUFFER_FLAG_TCP_SERVER 0x10

extern uint32_t Syslog_CircularBuffer_ForceWrite(SyslogCircularBuffer *buffer, void *data, uint32_t dataLen);
extern uint32_t Syslog_CircularBuffer_Read(SyslogCircularBuffer *buffer, void *dst, uint32_t destLen);
extern void (*const Syslog_SignalSyslogOutputThread)();
extern void (*const Syslog_FlushHistoryToOutput)(const SyslogCircularBuffer *buffer, uint32_t flags);

int syslogRouteThreadStart();

extern SyslogBuffers *gSyslogBuffers;
extern SyslogCircularBuffer *gActiveSyslogBuffer;
extern uint32_t gSyslogBufferFlags;

extern bool gSkipSyslogCatchUpForUSBOutput;

int TCPSyslogStartThread(bool useIPFilter, uint32_t allowedIP);

void TCPSyslogInit();
void TCPSyslogEndThread();