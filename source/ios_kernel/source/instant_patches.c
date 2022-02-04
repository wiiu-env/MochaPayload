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
#include "../../ios_mcp/ios_mcp_syms.h"
#include "elf_patcher.h"
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

#define mcp_rodata_phys(addr) ((u32) (addr) -0x05060000 + 0x08220000)
#define mcp_data_phys(addr)   ((u32) (addr) -0x05074000 + 0x08234000)
#define acp_phys(addr)        ((u32) (addr) -0xE0000000 + 0x12900000)

void instant_patches_setup(void) {
    // apply IOS ELF launch hook
    *(volatile u32 *) 0x0812A120 = ARM_BL(0x0812A120, kernel_launch_ios);

    *(volatile u32 *) 0x0812CD2C = ARM_B(0x0812CD2C, kernel_syscall_0x81);

    // patch FSA raw access
    *(volatile u32 *) 0x1070FAE8 = 0x05812070;
    *(volatile u32 *) 0x1070FAEC = 0xEAFFFFF9;

    // patch /dev/odm IOCTL 0x06 to return the disc key if in_buf[0] > 2.
    *(volatile u32 *) 0x10739948 = 0xe3a0b001; // mov r11, 0x01
    *(volatile u32 *) 0x1073994C = 0xe3a07020; // mov r7, 0x20
    *(volatile u32 *) 0x10739950 = 0xea000013; // b LAB_107399a8

    int (*_iosMapSharedUserExecution)(void *descr) = (void *) 0x08124F88;

    // patch kernel dev node registration
    *(volatile u32 *) 0x081430B4 = 1;

    // fix 10 minute timeout that crashes MCP after 10 minutes of booting
    *(volatile u32 *) (0x05022474 - 0x05000000 + 0x081C0000) = 0xFFFFFFFF; // NEW_TIMEOUT

    kernel_memset((void *) (0x050BD000 - 0x05000000 + 0x081C0000), 0, 0x2F00);

    // allow custom bootLogoTex and bootMovie.h264
    *(volatile u32 *) (0xE0030D68 - 0xE0000000 + 0x12900000) = 0xE3A00000; // mov r0, #0
    *(volatile u32 *) (0xE0030D34 - 0xE0000000 + 0x12900000) = 0xE3A00000; // mov r0, #0

    // Patch update check
    *(volatile u32 *) (0xe22830e0 - 0xe2280000 + 0x13140000) = 0x00000000;
    *(volatile u32 *) (0xe22b2a78 - 0xe2280000 + 0x13140000) = 0x00000000;
    *(volatile u32 *) (0xe204fb68 - 0xe2000000 + 0x12EC0000) = 0xe3a00000;

    // allow any region title launch
    *(volatile u32 *) (0xE0030498 - 0xE0000000 + 0x12900000) = 0xE3A00000; // mov r0, #0
    // Patch CheckTitleLaunch to ignore gamepad connected result
    *(volatile u32 *) (0xE0030868 - 0xE0000000 + 0x12900000) = 0xE3A00000; // mov r0, #0

    *(volatile u32 *) (0x050254D6 - 0x05000000 + 0x081C0000) = THUMB_BL(0x050254D6, MCP_LoadFile_patch);
    *(volatile u32 *) (0x05025242 - 0x05000000 + 0x081C0000) = THUMB_BL(0x05025242, MCP_ioctl100_patch);

    *(volatile u32 *) (0x0501dd78 - 0x05000000 + 0x081C0000) = THUMB_BL(0x0501dd78, MCP_ReadCOSXml_patch);
    *(volatile u32 *) (0x051105ce - 0x05000000 + 0x081C0000) = THUMB_BL(0x051105ce, MCP_ReadCOSXml_patch);

    // give us bsp::ee:read permission for PPC
    *(volatile u32 *) (0xe6044db0 - 0xe6042000 + 0x13d02000) = 0x000001F0;

    // Patch MCP debugmode check for syslog
    *(volatile u32 *) (0x050290d8 - 0x05000000 + 0x081C0000) = 0x20004770;
    // Patch TEST to allow syslog
    *(volatile u32 *) (0xe4007828 - 0xe4000000 + 0x13A40000) = 0xe3a00000;

    // patch default title id to system menu
    *(volatile u32 *) mcp_data_phys(0x050B817C) = *(volatile u32 *) 0x0017FFF0;
    *(volatile u32 *) mcp_data_phys(0x050B8180) = *(volatile u32 *) 0x0017FFF4;

    // Place the environment path at the end of our .text section.
    for (int i = 0; i < 0x100; i += 4) {
        *(volatile u32 *) (0x05119F00 - 0x05100000 + 0x13D80000 + i) = *(volatile u32 *) (0x0017FEF0 + i);
    }

    // force check USB storage on load
    *(volatile u32 *) acp_phys(0xE012202C) = 0x00000001; // find USB flag

    // set zero to start thread directly on first title change
    *(volatile u32 *) (0x050BC580 - 0x05000000 + 0x081C0000) = 0;

    ios_map_shared_info_t map_info;
    map_info.paddr  = 0x050BD000 - 0x05000000 + 0x081C0000;
    map_info.vaddr  = 0x050BD000;
    map_info.size   = 0x3000;
    map_info.domain = 1; // MCP
    map_info.type   = 3; // 0 = undefined, 1 = kernel only, 2 = read only, 3 = read/write
    map_info.cached = 0xFFFFFFFF;
    _iosMapSharedUserExecution(&map_info); // actually a bss section but oh well it will have read/write

    map_info.paddr  = 0x05116000 - 0x05100000 + 0x13D80000;
    map_info.vaddr  = 0x05116000;
    map_info.size   = 0x4000;
    map_info.domain = 1; // MCP
    map_info.type   = 3; // 0 = undefined, 1 = kernel only, 2 = read only, 3 = read write
    map_info.cached = 0xFFFFFFFF;
    _iosMapSharedUserExecution(&map_info);
}
