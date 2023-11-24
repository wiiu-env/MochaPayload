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
#include "ios_fs_patches.h"
#include "../../ios_fs/ios_fs_syms.h"
#include "elf_patcher.h"
#include "types.h"
#include <string.h>

#define FS_PHYS_DIFF 0

u32 fs_get_phys_code_base(void) {
    return _text_start + FS_PHYS_DIFF;
}

void reset_fs_bss() {
    memset((void *) _bss_start, 0, _bss_end - _bss_start);
}

void fs_run_patches(uint32_t ios_elf_start) {
    section_write(ios_elf_start, _text_start, (void *) fs_get_phys_code_base(), _text_end - _text_start);
    section_write_bss(ios_elf_start, _bss_start, _bss_end - _bss_start);

    // Add IOCTL 0x28 to indicate the calling client should have full fs permissions
    section_write_word(ios_elf_start, 0x10701248, _FSA_ioctl0x28_hook);

    // Give clients that called IOCTL 0x28 full permissions
    section_write_word(ios_elf_start, 0x10704540, ARM_BL(0x10704540, FSA_IOCTLV_HOOK));
    section_write_word(ios_elf_start, 0x107044f0, ARM_BL(0x107044f0, FSA_IOCTL_HOOK));
    section_write_word(ios_elf_start, 0x10704458, ARM_BL(0x10704458, FSA_IOS_Close_Hook));

    // Keep patches for backwards compatibility (libiosuhax)
    // patch FSA raw access
    section_write_word(ios_elf_start, 0x1070FAE8, 0x05812070);
    section_write_word(ios_elf_start, 0x1070FAEC, 0xEAFFFFF9);

    // fat32 driver patches

    // patch out fopen in stat
    section_write_word(ios_elf_start, 0x1078c748, 0xe3a00001); // mov r0, #1
    // patch out fclose in stat
    section_write_word(ios_elf_start, 0x1078c754, 0xe3a00000); // mov r0, #0
    // patch out diropen in stat
    section_write_word(ios_elf_start, 0x1078c71c, 0xe3a00001); // mov r0, #1
    // patch out dirclose in stat
    section_write_word(ios_elf_start, 0x1078c728, 0xe3a00000); // mov r0, #0

    // patch out fopen in readdir
    section_write_word(ios_elf_start, 0x1078b50c, 0xe3a00001); // mov r0, #1

    // patch out fclose in readdir
    section_write_word(ios_elf_start, 0x1078b518, 0xe3a00000); // mov r0, #0

    // patch out fopendir in readdir
    section_write_word(ios_elf_start, 0x1078b62c, 0xe3a00001); // mov r0, #1

    // patch out fclosedir in readdir
    section_write_word(ios_elf_start, 0x1078b638, 0xe3a00001); // mov r0, #1

    // Avoid opening the file for getting the "alloc_size" in stat and readdir
    section_write_word(ios_elf_start, 0x1078d5b0, 0xe3a00001); // mov r0, #1 // fopen
    section_write_word(ios_elf_start, 0x1078d738, 0xe3a00000); // mov r0, #0 // fclose
    section_write_word(ios_elf_start, 0x1078d5c4, 0xe3a00000); // mov r0, #0 // finfo

    // patch alloc_size to be filesize
    {
        // nop some code we don't want
        section_write_word(ios_elf_start, 0x1078d6e8, 0xea000000); // mov r0, r0
        section_write_word(ios_elf_start, 0x1078d6ec, 0xea000000); // mov r0, r0

        // set param_2->allocSize = param_3->size;
        section_write_word(ios_elf_start, 0x1078d6f0, 0xe5db3248); // ldrb r3,[r11,#0x248]
        section_write_word(ios_elf_start, 0x1078d6f4, 0xe5c93014); // strb r3,[r9,#0x14]
        section_write_word(ios_elf_start, 0x1078d6f8, 0xe5db3249); // ldrb r3,[r11,#0x249]
        section_write_word(ios_elf_start, 0x1078d6fc, 0xe5c93015); // strb r3,[r9,#0x15]
        section_write_word(ios_elf_start, 0x1078d700, 0xe5db324a); // ldrb r3,[r11,#0x24a]
        section_write_word(ios_elf_start, 0x1078d704, 0xe5c93016); // strb r3,[r9,#0x16]
        section_write_word(ios_elf_start, 0x1078d708, 0xe5db324b); // ldrb r3,[r11,#0x24b]
        section_write_word(ios_elf_start, 0x1078d70c, 0xe5c93017); // strb r3,[r9,#0x17]

        // nop previous alloc_size assign
        section_write_word(ios_elf_start, 0x1078d71c, 0xea000000); // mov r0, r0
        section_write_word(ios_elf_start, 0x1078d720, 0xea000000); // mov r0, r0
        section_write_word(ios_elf_start, 0x1078d724, 0xea000000); // mov r0, r0
    }
}