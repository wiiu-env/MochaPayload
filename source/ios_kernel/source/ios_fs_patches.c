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

#define FS_PHYS_DIFF 0

u32 fs_get_phys_code_base(void) {
    return _text_start + FS_PHYS_DIFF;
}

void fs_run_patches(u32 ios_elf_start) {
    section_write(ios_elf_start, _text_start, (void *) fs_get_phys_code_base(), _text_end - _text_start);
    section_write_bss(ios_elf_start, _bss_start, _bss_end - _bss_start);

    // Patch IOCTL 0x28 to give the calling client full fs permission
    section_write_word(ios_elf_start, 0x10701248, _FSA_ioctl0x28_hook);

    // patch FSA raw access
    section_write_word(ios_elf_start, 0x1070FAE8, 0x05812070);
    section_write_word(ios_elf_start, 0x1070FAEC, 0xEAFFFFF9);
}