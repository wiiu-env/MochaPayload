#pragma once

#include <assert.h>
#include <stdint.h>

#define FSA_CAPABILITY_ODD_READ         (1llu << 0)
#define FSA_CAPABILITY_ODD_WRITE        (1llu << 1)
#define FSA_CAPABILITY_ODD_RAW_OPEN     (1llu << 2)
#define FSA_CAPABILITY_ODD_MOUNT        (1llu << 3)
#define FSA_CAPABILITY_SLCCMPT_READ     (1llu << 4)
#define FSA_CAPABILITY_SLCCMPT_WRITE    (1llu << 5)
#define FSA_CAPABILITY_SLCCMPT_RAW_OPEN (1llu << 6)
#define FSA_CAPABILITY_SLCCMPT_MOUNT    (1llu << 7)
#define FSA_CAPABILITY_SLC_READ         (1llu << 8)
#define FSA_CAPABILITY_SLC_WRITE        (1llu << 9)
#define FSA_CAPABILITY_SLC_RAW_OPEN     (1llu << 10)
#define FSA_CAPABILITY_SLC_MOUNT        (1llu << 11)
#define FSA_CAPABILITY_MLC_READ         (1llu << 12)
#define FSA_CAPABILITY_MLC_WRITE        (1llu << 13)
#define FSA_CAPABILITY_MLC_RAW_OPEN     (1llu << 14)
#define FSA_CAPABILITY_MLC_MOUNT        (1llu << 15)
#define FSA_CAPABILITY_SDCARD_READ      (1llu << 16)
#define FSA_CAPABILITY_SDCARD_WRITE     (1llu << 17)
#define FSA_CAPABILITY_SDCARD_RAW_OPEN  (1llu << 18)
#define FSA_CAPABILITY_SDCARD_MOUNT     (1llu << 19)
#define FSA_CAPABILITY_HFIO_READ        (1llu << 20)
#define FSA_CAPABILITY_HFIO_WRITE       (1llu << 21)
#define FSA_CAPABILITY_HFIO_RAW_OPEN    (1llu << 22)
#define FSA_CAPABILITY_HFIO_MOUNT       (1llu << 23)
#define FSA_CAPABILITY_RAMDISK_READ     (1llu << 24)
#define FSA_CAPABILITY_RAMDISK_WRITE    (1llu << 25)
#define FSA_CAPABILITY_RAMDISK_RAW_OPEN (1llu << 26)
#define FSA_CAPABILITY_RAMDISK_MOUNT    (1llu << 27)
#define FSA_CAPABILITY_USB_READ         (1llu << 28)
#define FSA_CAPABILITY_USB_WRITE        (1llu << 29)
#define FSA_CAPABILITY_USB_RAW_OPEN     (1llu << 30)
#define FSA_CAPABILITY_USB_MOUNT        (1llu << 31)
#define FSA_CAPABILITY_OTHER_READ       (1llu << 32)
#define FSA_CAPABILITY_OTHER_WRITE      (1llu << 33)
#define FSA_CAPABILITY_OTHER_RAW_OPEN   (1llu << 34)
#define FSA_CAPABILITY_OTHER_MOUNT      (1llu << 35)

typedef struct __attribute__((packed)) {
    uint32_t initialized;
    uint64_t titleId;
    uint32_t processId;
    uint32_t groupId;
    uint32_t unk0;
    uint64_t capabilityMask;
    uint8_t unk1[0x4518];
    char unk2[0x280];
    char unk3[0x280];
    void *mutex;
} FSAProcessData;
static_assert(sizeof(FSAProcessData) == 0x4A3C, "FSAProcessData: wrong size");

typedef struct __attribute__((packed)) {
    uint32_t opened;
    FSAProcessData *processData;
    char unk0[0x10];
    char unk1[0x90];
    uint32_t unk2;
    char work_dir[0x280];
    uint32_t unk3;
} FSAClientHandle;
static_assert(sizeof(FSAClientHandle) == 0x330, "FSAClientHandle: wrong size");
