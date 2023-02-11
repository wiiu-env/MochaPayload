#pragma once

#include <assert.h>
#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#ifndef CHECK_SIZE
#define CHECK_SIZE(type, size) static_assert(sizeof(type) == size, #type " must be " #size " bytes")
#endif

typedef enum SmdState SmdState;
typedef enum SmdInterfaceState SmdInterfaceState;
typedef enum SmdElementType SmdElementType;
typedef struct SmdIopContext SmdIopContext;
typedef struct SmdInterface SmdInterface;
typedef struct SmdControlTable SmdControlTable;
typedef struct SmdVectorSpec SmdVectorSpec;
typedef struct SmdVector SmdVector;
typedef struct SmdElement SmdElement;
typedef struct SmdReceiveData SmdReceiveData;

enum SmdState {
    SMD_STATE_INVALID     = 0,
    SMD_STATE_INITIALIZED = 2,
    SMD_STATE_CLOSED      = 3,
    SMD_STATE_OPENED      = 4,
};

enum SmdInterfaceState {
    SMD_INTERFACE_STATE_OPENED = 0x2222,
    SMD_INTERFACE_STATE_CLOSED = 0x3333,
};

enum SmdElementType {
    SMD_ELEMENT_TYPE_MESSAGE     = 0,
    SMD_ELEMENT_TYPE_VECTOR_SPEC = 1,
    SMD_ELEMENT_TYPE_VECTOR      = 2,
};

struct SmdIopContext {
    SmdControlTable *controlTable;
    int32_t memVirtOffset;
    int32_t memPhysOffset;
    int shouldLock;
    int semaphore;
    int hasSemaphore;
    int logHandle;
    SmdElement *iopElementBuf;
    uint32_t iopElementCount;
    SmdElement *ppcElementBuf;
    uint32_t ppcElementCount;
    SmdState state;
};

struct PACKED SmdInterface {
    uint32_t state;
    uint8_t __padding0[0x7C];
    uint32_t elementCount;
    uint8_t __padding1[0x7C];
    int32_t readOffset;
    uint8_t __padding2[0x7C];
    int32_t writeOffset;
    uint8_t __padding3[0x7C];
    uint32_t bufPaddr;
    uint8_t __padding4[0x7C];
};
CHECK_SIZE(SmdInterface, 0x280);

struct PACKED SmdControlTable {
    char name[0x10];
    uint32_t reinitCount;
    uint8_t __padding0[0x6C];
    SmdInterface iopInterface;
    uint8_t __padding1[0x40];
    SmdInterface ppcInterface;
    uint8_t __padding2[0x40];
};
CHECK_SIZE(SmdControlTable, 0x600);

struct PACKED SmdVectorSpec {
    void *ptr;
    uint32_t size;
};
CHECK_SIZE(SmdVectorSpec, 0x8);

struct PACKED SmdVector {
    uint32_t id;
    int32_t count;
    SmdVectorSpec vecs[4];
};
CHECK_SIZE(SmdVector, 0x28);

struct PACKED SmdElement {
    uint32_t type;
    uint32_t size;
    union {
        uint8_t data[0xf8];
        SmdVector spec;
        uint32_t vectorPaddr;
    };
};
CHECK_SIZE(SmdElement, 0x100);

struct PACKED SmdReceiveData {
    uint32_t type;
    uint32_t size;
    union {
        uint8_t message[0x80];
        SmdVector spec;
        SmdVector *vector;
    };
};
CHECK_SIZE(SmdReceiveData, 0x88);

int32_t smdIopInit(int32_t index, SmdControlTable *table, const char *name, int lock);

int32_t smdIopOpen(int32_t index);

int32_t smdIopClose(int32_t index);

int32_t smdIopGetInterfaceState(int32_t index, SmdInterfaceState *iopState, SmdInterfaceState *ppcState);

int32_t smdIopReceive(int32_t index, SmdReceiveData *data);

int32_t smdIopSendVector(int32_t index, SmdVector *vector);
