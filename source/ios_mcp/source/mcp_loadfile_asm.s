.extern _MCP_LoadFile_patch
.global MCP_LoadFile_patch
MCP_LoadFile_patch:
    .thumb
    bx pc
    nop
    .arm
    ldr r12, =_MCP_LoadFile_patch
    bx r12

.extern _MCP_ioctl100_patch
.global MCP_ioctl100_patch
MCP_ioctl100_patch:
    .thumb
    ldr r0, [r7,#0xC]
    bx pc
    nop
    .arm
    ldr r12, =_MCP_ioctl100_patch
    bx r12

.extern _MCP_ReadCOSXml_patch
.global MCP_ReadCOSXml_patch
MCP_ReadCOSXml_patch:
    .thumb
    bx pc
    nop
    .arm
    ldr r12, =_MCP_ReadCOSXml_patch
    bx r12

.extern _Syslog_RouteOutputPatch
.global Syslog_RouteOutputPatch
Syslog_RouteOutputPatch:
    .thumb
    bx pc
    nop
    .arm
    ldr r12, =_Syslog_RouteOutputPatch
    bx r12

.extern _Syslog_FlushHistoryToOutputForUSB
.global Syslog_FlushHistoryToOutputForUSB
Syslog_FlushHistoryToOutputForUSB:
    .thumb
    bx pc
    nop
    .arm
    ldr r12, =_Syslog_FlushHistoryToOutputForUSB
    bx r12

