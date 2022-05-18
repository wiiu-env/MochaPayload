#include <stdint.h>
#include <stdio.h>

typedef struct __attribute__((packed)) {
    uint64_t titleId;
    uint32_t processId;
    uint32_t groupId;
    uint32_t unk0;
    uint32_t unk1;
    uint64_t capabilityMask;
    uint32_t unk2;
} FSAClientCapabilites;

typedef struct __attribute__((packed)) {
    uint32_t opened;
    FSAClientCapabilites* caps;
    char unk0[0x10];
    char unk1[0x90];
    uint32_t unk2;
    char work_dir[0x280];
    uint32_t unk3;
} FSAClientHandle;

int FSA_ioctl0x28_hook(FSAClientHandle* handle)
{
    printf("Updating client capabilities for handle %p\n", handle);
    handle->caps->capabilityMask = 0xffffffffffffffff;
    return 0;
}
