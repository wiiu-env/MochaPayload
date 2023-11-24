/***************************************************************************
 * Copyright (C) 2016
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "../../ios_fs/ios_fs_syms.h"
#include "../../ios_mcp/ios_mcp_syms.h"
#include "elf_patcher.h"
#include "ios_fs_patches.h"
#include "ios_mcp_patches.h"
#include "kernel_patches.h"
#include "types.h"
#include "utils.h"

typedef struct {
    u32 paddr;
    u32 vaddr;
    u32 size;
    u32 domain;
    u32 type;
    u32 cached;
} ios_map_shared_info_t;
#define MCP_CUSTOM_TEXT_LENGTH     0xA000
#define MCP_CUSTOM_TEXT_START      0x05116000
#define MCP_CUSTOM_BSS_START       0x050BD000
#define MCP_CUSTOM_BSS_LENGTH      0x3000
#define ENVIRONMENT_PATH_LENGTH    0x100

#define mcp_text_phys(addr)        ((u32) (addr) -0x05000000 + 0x081C0000)
#define mcp_rodata_phys(addr)      ((u32) (addr) -0x05060000 + 0x08220000)
#define mcp_data_phys(addr)        ((u32) (addr) -0x05074000 + 0x08234000)
#define fsa_phys(addr)             ((u32) (addr))
#define kernel_phys(addr)          ((u32) (addr))
#define acp_text_phys(addr)        ((u32) (addr) -0xE0000000 + 0x12900000)
#define nimboss_text_phys(addr)    ((u32) (addr) -0xe2000000 + 0x12EC0000)
#define nimboss_rodata_phys(addr)  ((u32) (addr) -0xe2280000 + 0x13140000)
#define bsp_data_phys(addr)        ((u32) (addr) -0xe6042000 + 0x13d02000)
#define mcp_custom_text_phys(addr) ((u32) (addr) -0x05100000 + 0x13D80000)
#define mcp_custom_bss_phys(addr)  ((u32) (addr) -0x05000000 + 0x081C0000)

void instant_patches_setup(void) {
    // apply IOS ELF launch hook
    *(volatile u32 *) 0x0812A120 = ARM_BL(0x0812A120, kernel_launch_ios);

    *(volatile u32 *) 0x0812CD2C = ARM_B(0x0812CD2C, kernel_syscall_0x81);

    // Keep patches for backwards compatibility (libiosuhax)
    // patch FSA raw access
    *(volatile u32 *) fsa_phys(0x1070FAE8) = 0x05812070;
    *(volatile u32 *) fsa_phys(0x1070FAEC) = 0xEAFFFFF9;

    // Add IOCTL 0x28 to indicate the calling client should have full fs permissions
    *(volatile u32 *) fsa_phys(0x10701248) = _FSA_ioctl0x28_hook;

    // Give clients that called IOCTL 0x28 full permissions
    *(volatile u32 *) fsa_phys(0x10704540) = ARM_BL(0x10704540, FSA_IOCTLV_HOOK);
    *(volatile u32 *) fsa_phys(0x107044f0) = ARM_BL(0x107044f0, FSA_IOCTL_HOOK);
    *(volatile u32 *) fsa_phys(0x10704458) = ARM_BL(0x10704458, FSA_IOS_Close_Hook);

    reset_fs_bss();

    // patch /dev/odm IOCTL 0x06 to return the disc key if in_buf[0] > 2.
    *(volatile u32 *) fsa_phys(0x10739948) = 0xe3a0b001; // mov r11, 0x01
    *(volatile u32 *) fsa_phys(0x1073994C) = 0xe3a07020; // mov r7, 0x20
    *(volatile u32 *) fsa_phys(0x10739950) = 0xea000013; // b LAB_107399a8

    // fat32 driver patches
    {
        // patch out fopen in stat
        *(volatile u32 *) fsa_phys(0x1078c748) = 0xe3a00001; //mov r0, #1
        // patch out fclose in stat
        *(volatile u32 *) fsa_phys(0x1078c754) = 0xe3a00000; //mov r0, #0
        // patch out diropen in stat
        *(volatile u32 *) fsa_phys(0x1078c71c) = 0xe3a00001; //mov r0, #1
        // patch out dirclose in stat
        *(volatile u32 *) fsa_phys(0x1078c728) = 0xe3a00000; //mov r0, #0

        // patch out fopen in readdir
        *(volatile u32 *) fsa_phys(0x1078b50c) = 0xe3a00001; //mov r0, #1

        // patch out fclose in readdir
        *(volatile u32 *) fsa_phys(0x1078b518) = 0xe3a00000; //mov r0, #0

        // patch out fopendir in readdir
        *(volatile u32 *) fsa_phys(0x1078b62c) = 0xe3a00001; //mov r0, #1

        // patch out fclosedir in readdir
        *(volatile u32 *) fsa_phys(0x1078b638) = 0xe3a00001; //mov r0, #1

        // Avoid opening the file for getting the "alloc_size" in stat and readdir
        *(volatile u32 *) fsa_phys(0x1078d5b0) = 0xe3a00001; //mov r0, #1 // fopen
        *(volatile u32 *) fsa_phys(0x1078d738) = 0xe3a00000; //mov r0, #0 // fclose
        *(volatile u32 *) fsa_phys(0x1078d5c4) = 0xe3a00000; //mov r0, #0 // finfo

        // patch alloc_size to be filesize
        {
            // nop some code we don't want
            *(volatile u32 *) fsa_phys(0x1078d6e8) = 0xea000000; //mov r0, r0
            *(volatile u32 *) fsa_phys(0x1078d6ec) = 0xea000000; //mov r0, r0

            // set param_2->allocSize = param_3->size;
            *(volatile u32 *) fsa_phys(0x1078d6f0) = 0xe5db3248; //ldrb r3,[r11,#0x248]
            *(volatile u32 *) fsa_phys(0x1078d6f4) = 0xe5c93014; //strb r3,[r9,#0x14]
            *(volatile u32 *) fsa_phys(0x1078d6f8) = 0xe5db3249; //ldrb r3,[r11,#0x249]
            *(volatile u32 *) fsa_phys(0x1078d6fc) = 0xe5c93015; //strb r3,[r9,#0x15]
            *(volatile u32 *) fsa_phys(0x1078d700) = 0xe5db324a; //ldrb r3,[r11,#0x24a]
            *(volatile u32 *) fsa_phys(0x1078d704) = 0xe5c93016; //strb r3,[r9,#0x16]
            *(volatile u32 *) fsa_phys(0x1078d708) = 0xe5db324b; //ldrb r3,[r11,#0x24b]
            *(volatile u32 *) fsa_phys(0x1078d70c) = 0xe5c93017; //strb r3,[r9,#0x17]

            // nop previous alloc_size assign
            *(volatile u32 *) fsa_phys(0x1078d71c) = 0xea000000; //mov r0, r0
            *(volatile u32 *) fsa_phys(0x1078d720) = 0xea000000; //mov r0, r0
            *(volatile u32 *) fsa_phys(0x1078d724) = 0xea000000; //mov r0, r0
        }
    }

    int (*_iosMapSharedUserExecution)(void *descr) = (void *) 0x08124F88;

    // patch kernel dev node registration
    *(volatile u32 *) kernel_phys(0x081430B4) = 1;

    // fix 10 minute timeout that crashes MCP after 10 minutes of booting
    *(volatile u32 *) mcp_text_phys(0x05022474) = 0xFFFFFFFF; // NEW_TIMEOUT

    kernel_memset((void *) mcp_custom_bss_phys(0x050BD000), 0, MCP_CUSTOM_BSS_LENGTH);

    // allow custom bootLogoTex and bootMovie.h264
    *(volatile u32 *) acp_text_phys(0xE0030D68) = 0xE3A00000; // mov r0, #0
    *(volatile u32 *) acp_text_phys(0xE0030D34) = 0xE3A00000; // mov r0, #0

    // Patch update check
    *(volatile u32 *) nimboss_rodata_phys(0xe22830e0) = 0x00000000;
    *(volatile u32 *) nimboss_rodata_phys(0xe22b2a78) = 0x00000000;
    *(volatile u32 *) nimboss_text_phys(0xe204fb68)   = 0xe3a00000;

    // allow any region title launch
    *(volatile u32 *) acp_text_phys(0xE0030498) = 0xE3A00000; // mov r0, #0
    // Patch CheckTitleLaunch to ignore gamepad connected result
    *(volatile u32 *) acp_text_phys(0xE0030868) = 0xE3A00000; // mov r0, #0

    *(volatile u32 *) mcp_text_phys(0x050254D6) = THUMB_BL(0x050254D6, MCP_LoadFile_patch);
    *(volatile u32 *) mcp_text_phys(0x05025242) = THUMB_BL(0x05025242, MCP_ioctl100_patch);

    *(volatile u32 *) mcp_text_phys(0x0501dd78) = THUMB_BL(0x0501dd78, MCP_ReadCOSXml_patch);
    *(volatile u32 *) mcp_text_phys(0x051105ce) = THUMB_BL(0x051105ce, MCP_ReadCOSXml_patch);

    // give us bsp::ee:read permission for PPC
    *(volatile u32 *) bsp_data_phys(0xe6044db0) = 0x000001F0;

    // patch default title id to system menu
    *(volatile u32 *) mcp_data_phys(0x050B817C) = *(volatile u32 *) 0x0017FFF0;
    *(volatile u32 *) mcp_data_phys(0x050B8180) = *(volatile u32 *) 0x0017FFF4;

    // Place the environment path at the end of our .text section.
    for (int i = 0; i < ENVIRONMENT_PATH_LENGTH; i += 4) {
        *(volatile u32 *) mcp_custom_text_phys(MCP_CUSTOM_TEXT_START + MCP_CUSTOM_TEXT_LENGTH - ENVIRONMENT_PATH_LENGTH + i) = *(volatile u32 *) (0x0017FEF0 + i);
    }

    // force check USB storage on load
    *(volatile u32 *) acp_text_phys(0xE012202C) = 0x00000001; // find USB flag

    // Patch FS to syslog everything
    *(volatile u32 *) fsa_phys(0x107F5720) = ARM_B(0x107F5720, 0x107F0C84);

    // Patch MCP to syslog everything
    *(volatile u32 *) mcp_text_phys(0x05055438) = ARM_B(0x05055438, 0x0503dcf8);

    ios_map_shared_info_t map_info;
    map_info.paddr  = mcp_custom_bss_phys(MCP_CUSTOM_BSS_START);
    map_info.vaddr  = MCP_CUSTOM_BSS_START;
    map_info.size   = MCP_CUSTOM_BSS_LENGTH;
    map_info.domain = 1; // MCP
    map_info.type   = 3; // 0 = undefined, 1 = kernel only, 2 = read only, 3 = read/write
    map_info.cached = 0xFFFFFFFF;
    _iosMapSharedUserExecution(&map_info); // actually a bss section but oh well it will have read/write

    map_info.paddr  = mcp_custom_text_phys(MCP_CUSTOM_TEXT_START);
    map_info.vaddr  = MCP_CUSTOM_TEXT_START;
    map_info.size   = MCP_CUSTOM_TEXT_LENGTH;
    map_info.domain = 1; // MCP
    map_info.type   = 3; // 0 = undefined, 1 = kernel only, 2 = read only, 3 = read write
    map_info.cached = 0xFFFFFFFF;
    _iosMapSharedUserExecution(&map_info);
}
