.arm

#patch_os_launch_sig_check:
#	.thumb
#	mov r0, #0
#	mov r0, #0

patch_wfs_partition_check:
	.thumb
	mov r0, #0
	mov r0, #0


patch_bootMovie_check:
patch_bootLogoTex_check:
patch_region_launch_check:
patch_gamepad_launch_check:
	.arm
	mov r0, #0
	bx lr


.globl mcp_patches_table, mcp_patches_table_end
mcp_patches_table:
#          origin           data                                    size
#     .word 0x0502ADF6,      patch_wfs_partition_check,              4
#     .word 0x05014AD8,      patch_wfs_partition_check,              4
# over an hour, MCP crash prevention
     .word 0x05022474,      0xFFFFFFFF,                             4
# MCP patches end here actually but lets treat the ACP patches as MCP as there are only patches
     .word 0xE0030D68,      patch_bootMovie_check,                  4
     .word 0xE0030D34,      patch_bootLogoTex_check,                4
     .word 0xE0030498,      patch_region_launch_check,              4
     .word 0xE0030868,      patch_gamepad_launch_check,             4
mcp_patches_table_end:

