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
    uint64_t title_id;
    uint64_t boss_id;
    uint64_t os_version;
    uint64_t app_size;
    uint64_t common_save_size;
    uint64_t account_save_size;
    uint64_t common_boss_size;
    uint64_t account_boss_size;
    uint64_t join_game_mode_mask;
    uint32_t version;
    char product_code[32];
    char content_platform[32];
    char company_code[8];
    char mastering_date[32];
    uint32_t logo_type;
    uint32_t app_launch_type;
    uint32_t invisible_flag;
    uint32_t no_managed_flag;
    uint32_t no_event_log;
    uint32_t no_icon_database;
    uint32_t launching_flag;
    uint32_t install_flag;
    uint32_t closing_msg;
    uint32_t title_version;
    uint32_t group_id;
    uint32_t save_no_rollback;
    uint32_t bg_daemon_enable;
    uint32_t join_game_id;
    uint32_t olv_accesskey;
    uint32_t wood_tin;
    uint32_t e_manual;
    uint32_t e_manual_version;
    uint32_t region;
    uint32_t pc_cero;
    uint32_t pc_esrb;
    uint32_t pc_bbfc;
    uint32_t pc_usk;
    uint32_t pc_pegi_gen;
    uint32_t pc_pegi_fin;
    uint32_t pc_pegi_prt;
    uint32_t pc_pegi_bbfc;
    uint32_t pc_cob;
    uint32_t pc_grb;
    uint32_t pc_cgsrr;
    uint32_t pc_oflc;
    uint32_t pc_reserved0;
    uint32_t pc_reserved1;
    uint32_t pc_reserved2;
    uint32_t pc_reserved3;
    uint32_t ext_dev_nunchaku;
    uint32_t ext_dev_classic;
    uint32_t ext_dev_urcc;
    uint32_t ext_dev_board;
    uint32_t ext_dev_usb_keyboard;
    uint32_t ext_dev_etc;
    char ext_dev_etc_name[512];
    uint32_t eula_version;
    uint32_t drc_use;
    uint32_t network_use;
    uint32_t online_account_use;
    uint32_t direct_boot;
    uint32_t reserved_flag0;
    uint32_t reserved_flag1;
    uint32_t reserved_flag2;
    uint32_t reserved_flag3;
    uint32_t reserved_flag4;
    uint32_t reserved_flag5;
    uint32_t reserved_flag6;
    uint32_t reserved_flag7;
    char longname_ja[512];
    char longname_en[512];
    char longname_fr[512];
    char longname_de[512];
    char longname_it[512];
    char longname_es[512];
    char longname_zhs[512];
    char longname_ko[512];
    char longname_nl[512];
    char longname_pt[512];
    char longname_ru[512];
    char longname_zht[512];
    char shortname_ja[256];
    char shortname_en[256];
    char shortname_fr[256];
    char shortname_de[256];
    char shortname_it[256];
    char shortname_es[256];
    char shortname_zhs[256];
    char shortname_ko[256];
    char shortname_nl[256];
    char shortname_pt[256];
    char shortname_ru[256];
    char shortname_zht[256];
    char publisher_ja[256];
    char publisher_en[256];
    char publisher_fr[256];
    char publisher_de[256];
    char publisher_it[256];
    char publisher_es[256];
    char publisher_zhs[256];
    char publisher_ko[256];
    char publisher_nl[256];
    char publisher_pt[256];
    char publisher_ru[256];
    char publisher_zht[256];
    uint32_t add_on_unique_id[32];
} ACPMetaXml;

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

#define IPC_CUSTOM_START_MCP_THREAD       0xFE
#define IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED 0xFD
#define IPC_CUSTOM_LOAD_CUSTOM_RPX        0xFC
#define IPC_CUSTOM_META_XML_READ          0xFB
#define IPC_CUSTOM_START_USB_LOGGING      0xFA
#define IPC_CUSTOM_COPY_ENVIRONMENT_PATH  0xF9

#define LOAD_FILE_TARGET_SD_CARD          0
