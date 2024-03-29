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
#ifndef _KERNEL_PATCHES_H
#define _KERNEL_PATCHES_H

#include "types.h"

int kernel_init_otp_buffer(u32 sd_sector, int tagValid);

int kernel_syscall_0x81(u32 command, u32 arg1, u32 arg2, u32 arg3);

void kernel_launch_ios(u32 launch_address, u32 L, u32 C, u32 H);

void kernel_run_patches(u32 ios_elf_start);

#endif
