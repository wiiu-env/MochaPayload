.arm

.extern FSA_ioctl0x28_hook

.global _FSA_ioctl0x28_hook
_FSA_ioctl0x28_hook:
    mov r0, r10
    mov r1, r11
    bl FSA_ioctl0x28_hook
    mov r5,#0x0
    ldr pc, =0x10701194
