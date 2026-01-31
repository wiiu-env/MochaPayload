#ifndef _INSTANT_PATCHES_COMMON_H_
#define _INSTANT_PATCHES_COMMON_H_

#include "types.h"

#define MCP_CUSTOM_TEXT_LENGTH     0xA000
#define MCP_CUSTOM_TEXT_START      0x05116000
#define MCP_CUSTOM_BSS_START       0x050BD000
#define MCP_CUSTOM_BSS_LENGTH      0x3000
#define mcp_custom_text_phys(addr) ((u32) (addr) -0x05100000 + 0x13D80000)
#define mcp_custom_bss_phys(addr)  ((u32) (addr) -0x05000000 + 0x081C0000)

#endif
