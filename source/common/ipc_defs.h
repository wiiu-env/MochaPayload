#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define CHECK_SIZE(Type, Size)          \
    static_assert(sizeof(Type) == Size, \
                  #Type " must be " #Size " bytes")

#define CHECK_OFFSET(Type, Offset, Field)          \
    static_assert(offsetof(Type, Field) == Offset, \
                  #Type "::" #Field " must be at offset " #Offset)

typedef struct __attribute__((packed)) {
    uint32_t group;
    uint64_t mask;
} Permission;

typedef struct __attribute__((packed)) {

    uint32_t version;
    char unkn1[8];
    uint64_t titleId;
    uint32_t groupId;
    uint32_t cmdFlags;
    char argstr[4096];
    char *argv[64];
    uint32_t max_size;
    uint32_t avail_size;
    uint32_t codegen_size;
    uint32_t codegen_core;
    uint32_t max_codesize;
    uint32_t overlay_arena;
    uint32_t num_workarea_heap_blocks;
    uint32_t num_codearea_heap_blocks;
    Permission permissions[19];
    uint32_t default_stack0_size;
    uint32_t default_stack1_size;
    uint32_t default_stack2_size;
    uint32_t default_redzone0_size;
    uint32_t default_redzone1_size;
    uint32_t default_redzone2_size;
    uint32_t exception_stack0_size;
    uint32_t exception_stack1_size;
    uint32_t exception_stack2_size;
    uint32_t sdkVersion;
    uint32_t titleVersion;
    char unknwn2[0x1270 - 0x124C];
} MCPPPrepareTitleInfo;
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x00, version);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x0C, titleId);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x14, groupId);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x18, cmdFlags);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1C, argstr);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x101C, argv);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x111C, max_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1120, avail_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1124, codegen_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1128, codegen_core);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x112C, max_codesize);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1130, overlay_arena);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1134, num_workarea_heap_blocks);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1138, num_codearea_heap_blocks);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x113C, permissions);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1220, default_stack0_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1224, default_stack1_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1228, default_stack2_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x122C, default_redzone0_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1230, default_redzone1_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1234, default_redzone2_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1238, exception_stack0_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x123C, exception_stack1_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1240, exception_stack2_size);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1244, sdkVersion);
CHECK_OFFSET(MCPPPrepareTitleInfo, 0x1248, titleVersion);
CHECK_SIZE(MCPPPrepareTitleInfo, 0x1270);

typedef struct {
    uint32_t version;
    uint64_t os_version;
    uint64_t titleId;
    uint32_t titleVersion;
    uint32_t sdkVersion;
    uint32_t groupid;
    uint32_t app_type;
    uint32_t common_id;
} MCPAppXml;

typedef enum {
    //Load from the process's code directory (process title id)/code/%s
    LOAD_FILE_PROCESS_CODE = 0,
    //Load from the CafeOS directory (00050010-1000400A)/code/%s
    LOAD_FILE_CAFE_OS = 1,
    //Load from a system data title's content directory (0005001B-x)/content/%s
    LOAD_FILE_SYS_DATA_CONTENT = 2,
    //Load from a system data title's code directory (0005001B-x)/content/%s
    LOAD_FILE_SYS_DATA_CODE = 3,

    LOAD_FILE_FORCE_SIZE = 0xFFFFFFFF,
} MCPFileType;

typedef struct {
    unsigned char unk[0x10];

    unsigned int pos;
    MCPFileType type;
    unsigned int cafe_pid;

    unsigned char unk2[0xC];

    char name[0x40];

    unsigned char unk3[0x12D8 - 0x68];
} MCPLoadFileRequest;

#define MOCHA_API_VERSION 1