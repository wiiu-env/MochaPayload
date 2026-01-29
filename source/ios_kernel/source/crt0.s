.section ".init"
.arm
.align 4

.extern _main
.type _main, %function

_start:
	mov r0, #0
	b _main
