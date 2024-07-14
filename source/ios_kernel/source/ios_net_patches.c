#include "ios_net_patches.h"
#include "../../ios_net/ios_net_syms.h"
#include "elf_patcher.h"
#include "types.h"

#define NET_CODE_BASE_PHYS_ADDR (0)

u32 net_get_phys_code_base(void) {
    return _text_start + NET_CODE_BASE_PHYS_ADDR;
}

void net_run_patches(u32 ios_elf_start) {
    section_write(ios_elf_start, _text_start, (void *) net_get_phys_code_base(), _text_end - _text_start);

    // Patch DLP region check by replacing result code with success
    section_write_word(ios_elf_start, 0x1239DA7C, 0);

    // Patch DLP to ignore error for missing title archive
    section_write_word(ios_elf_start, 0x1239E108, 0xEA000000); // mov r0, r0
    section_write_word(ios_elf_start, 0x1239E10C, 0xEA000000); // mov r0, r0
    section_write_word(ios_elf_start, 0x1239E110, 0xEA000000); // mov r0, r0

    // Patch DLP path from /vol/content/dlp/app to sd:/dlp/app
    section_write_word(ios_elf_start, 0x12455368, 0x2F766F6C);      // /vol
    section_write_word(ios_elf_start, 0x12455368 + 4, 0x2F646C70);  // /dlp
    section_write_word(ios_elf_start, 0x12455368 + 8, 0x5F5F7364);  // __sd
    section_write_word(ios_elf_start, 0x12455368 + 12, 0x2F646C70); // /dlp
    section_write_word(ios_elf_start, 0x12455368 + 16, 0x2F617070); // /app
    section_write_word(ios_elf_start, 0x12455368 + 20, 0x00000000); //

    // Patch DLP path from /vol/content/dlp/app to sd:/dlp/app
    section_write_word(ios_elf_start, 0x12455490, 0x2F766F6C);      // /vol
    section_write_word(ios_elf_start, 0x12455490 + 4, 0x2F646C70);  // /dlp
    section_write_word(ios_elf_start, 0x12455490 + 8, 0x5F5F7364);  // __sd
    section_write_word(ios_elf_start, 0x12455490 + 12, 0x2F646C70); // /dlp
    section_write_word(ios_elf_start, 0x12455490 + 16, 0x2F617070); // /app
    section_write_word(ios_elf_start, 0x12455490 + 20, 0x00000000); //

    // DLP: (un)mount sd card for .cia reading.
    section_write_word(ios_elf_start, 0x1237f33c, ARM_BL(0x1237f33c, DLP_FSAInit_patch));
    section_write_word(ios_elf_start, 0x123a4448, ARM_BL(0x123a4448, DLP_FSAInit_patch));
    section_write_word(ios_elf_start, 0x1239de98, ARM_BL(0x1239de98, DLP_FSAInit_patch));

    section_write_word(ios_elf_start, 0x1237f310, ARM_BL(0x1237f310, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239dfa0, ARM_BL(0x1239dfa0, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239dfc0, ARM_BL(0x1239dfc0, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239dfd8, ARM_BL(0x1239dfd8, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239dfec, ARM_BL(0x1239dfec, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239e020, ARM_BL(0x1239e020, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x1239e094, ARM_BL(0x1239e094, DLP_FSADeinit_patch));
    section_write_word(ios_elf_start, 0x123a457c, ARM_BL(0x123a457c, DLP_FSADeinit_patch));

    // DLP debug:
    /*
    section_write_word(ios_elf_start, 0x123a449c, ARM_BL(0x123a449c, DLP_FSA_OpenFile));
    section_write_word(ios_elf_start, 0x1239ce08, ARM_BL(0x1239ce08, DLP_FSA_OpenFile));
    section_write_word(ios_elf_start, 0x1239cf68, ARM_BL(0x1239cf68, DLP_FSA_OpenFile));
    section_write_word(ios_elf_start, 0x1239defc, ARM_BL(0x1239defc, DLP_FSA_OpenFile));
    section_write_word(ios_elf_start, 0x1239debc, ARM_BL(0x1239debc, DLP_GetChildTitleId));
     */
}
