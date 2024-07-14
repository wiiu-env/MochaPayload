#ifndef _NET_PATCHES_H_
#define _NET_PATCHES_H_

#include "types.h"

u32 net_get_phys_code_base(void);

void net_run_patches(u32 ios_elf_start);

#endif
