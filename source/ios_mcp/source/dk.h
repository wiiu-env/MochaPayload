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

enum DKPCHARCommand {
    DK_PCHAR_COMMAND_READ  = 0x40,
    DK_PCHAR_COMMAND_WRITE = 0x41,
};

typedef struct PACKED {
    uint32_t command;
    uint8_t __unk[0x8];
    char path[0x20];
    uint8_t __padding[0x4];
    char smdWriteName[0x10];
    char smdReadName[0x10];
} DKPCHARActivateParams;
CHECK_SIZE(DKPCHARActivateParams, 0x50);

typedef struct PACKED {
    int32_t error;
    uint32_t param0;
    uint32_t param1;
} DKPCHARResult;
CHECK_SIZE(DKPCHARResult, 0xC);

typedef struct PACKED {
    uint32_t command;
    void *message;
    uint32_t unk;
    void *buf;
    uint32_t size;
    uint8_t __padding[0x3C];
} DKPCHARResponse;
CHECK_SIZE(DKPCHARResponse, 0x50);

int dkPCHARActivateSmd(IOSVec *vecs);
