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
#include "kernel_patches.h"
#include "../../common/kernel_commands.h"
#include "elf_patcher.h"
#include "fsa.h"
#include "ios_mcp_patches.h"
#include "thread.h"
#include "types.h"
#include "utils.h"

extern void __KERNEL_CODE_START(void);

extern void __KERNEL_CODE_END(void);

extern const patch_table_t kernel_patches_table[];
extern const patch_table_t kernel_patches_table_end[];

static const u32 mcpIoMappings_patch[] =
        {
                // vaddr    paddr       size        ?           ?           ?
                0x0D000000, 0x0D000000, 0x001C0000, 0x00000000, 0x00000003, 0x00000000, // mapping 1
                0x0D800000, 0x0D800000, 0x001C0000, 0x00000000, 0x00000003, 0x00000000, // mapping 2
                0x0C200000, 0x0C200000, 0x00100000, 0x00000000, 0x00000003, 0x00000000  // mapping 3
};

static const u32 KERNEL_MCP_IOMAPPINGS_STRUCT[] =
        {
                (u32) mcpIoMappings_patch, // ptr to iomapping structs
                0x00000003,                // number of iomappings
                0x00000001                 // pid (MCP)
};

ThreadContext_t **currentThreadContext = (ThreadContext_t **) 0x08173ba0;
uint32_t *domainAccessPermissions      = (uint32_t *) 0x081a4000;

int kernel_syscall_0x81(u32 command, u32 arg1, u32 arg2, u32 arg3) {
    int result = 0;
    int level  = disable_interrupts();
    set_domain_register(domainAccessPermissions[0]); // 0 = KERNEL

    switch (command) {
        case KERNEL_READ32: {
            result = *(volatile u32 *) arg1;
            break;
        }
        case KERNEL_WRITE32: {
            *(volatile u32 *) arg1 = arg2;
            break;
        }
        case KERNEL_MEMCPY: {
            kernel_memcpy((void *) arg1, (void *) arg2, arg3);
            break;
        }
        case KERNEL_GET_CFW_CONFIG: {
            //kernel_memcpy((void*)arg1, &cfw_config, sizeof(cfw_config));
            break;
        }
        case KERNEL_READ_OTP: {
            int (*read_otp_internal)(int index, void *out_buf, u32 size) = (int (*)(int, void *, u32)) 0x08120248;
            read_otp_internal(0, (void *) (arg1), 0x400);
            break;
        }
        default: {
            result = -1;
            break;
        }
    }

    set_domain_register(domainAccessPermissions[(*currentThreadContext)->pid]);
    enable_interrupts(level);

    return result;
}

void kernel_launch_ios(u32 launch_address, u32 L, u32 C, u32 H) {
    void (*kernel_launch_bootrom)(u32 launch_address, u32 L, u32 C, u32 H) = (void *) 0x0812A050;

    if (*(u32 *) (launch_address - 0x300 + 0x1AC) == 0x00DFD000) {
        int level                     = disable_interrupts();
        unsigned int control_register = disable_mmu();

        u32 ios_elf_start = launch_address + 0x804 - 0x300;

        //! try to keep the order of virt. addresses to reduce the memmove amount
        mcp_run_patches(ios_elf_start);
        kernel_run_patches(ios_elf_start);

        restore_mmu(control_register);
        enable_interrupts(level);
    }

    kernel_launch_bootrom(launch_address, L, C, H);
}

void kernel_run_patches(u32 ios_elf_start) {
    section_write(ios_elf_start, (u32) __KERNEL_CODE_START, __KERNEL_CODE_START, __KERNEL_CODE_END - __KERNEL_CODE_START);
    section_write_word(ios_elf_start, 0x0812A120, ARM_BL(0x0812A120, kernel_launch_ios));

    section_write(ios_elf_start, 0x08140DE0, KERNEL_MCP_IOMAPPINGS_STRUCT, sizeof(KERNEL_MCP_IOMAPPINGS_STRUCT));

    // patch /dev/odm IOCTL 0x06 to return the disc key if in_buf[0] > 2.
    section_write_word(ios_elf_start, 0x10739948, 0xe3a0b001);
    section_write_word(ios_elf_start, 0x1073994C, 0xe3a07020);
    section_write_word(ios_elf_start, 0x10739950, 0xea000013);

    // update check
    section_write_word(ios_elf_start, 0xe22830e0, 0x00000000);
    section_write_word(ios_elf_start, 0xe22b2a78, 0x00000000);
    section_write_word(ios_elf_start, 0xe204fb68, 0xe3a00000);

    // patch MCP syslog debug mode check
    section_write_word(ios_elf_start, 0x050290d8, 0x20004770);

    // Write magic word to disable custom IPC
    section_write_word(ios_elf_start, 0x050290dc, 0x42424242);

    // give us bsp::ee:read permission for PPC
    section_write_word(ios_elf_start, 0xe6044db0, 0x000001F0);

    // patch TEST debug mode check
    //section_write_word(ios_elf_start, 0xe4016a78, 0xe3a00000);
    section_write_word(ios_elf_start, 0xe4007828, 0xe3a00000);

    // Patch FS to syslog everything
    section_write_word(ios_elf_start, 0x107F5720, ARM_B(0x107F5720, 0x107F0C84));

    // Patch MCP to syslog everything
    section_write_word(ios_elf_start, 0x05055438, ARM_B(0x05055438, 0x0503dcf8));

    section_write_word(ios_elf_start, 0x0812CD2C, ARM_B(0x0812CD2C, kernel_syscall_0x81));

    u32 patch_count = (u32) (((u8 *) kernel_patches_table_end) - ((u8 *) kernel_patches_table)) / sizeof(patch_table_t);
    patch_table_entries(ios_elf_start, kernel_patches_table, patch_count);
}
