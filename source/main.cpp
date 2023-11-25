#include "common/ipc_defs.h"
#include "ios_exploit.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/ios.h>
#include <coreinit/thread.h>
#include <cstdio>
#include <cstring>
#include <mocha/commands.h>
#include <sysapp/title.h>
#include <whb/log.h>
#include <whb/log_udp.h>

static void StartMCPThreadIfMochaAlreadyRunning() {
    // start /dev/iosuhax and wupserver if mocha is already running
    int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
    if (mcpFd >= 0) {
        int in  = IPC_CUSTOM_START_MCP_THREAD;
        int out = 0;
        if (IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out)) == IOS_ERROR_OK) {
            // give /dev/iosuhax a chance to start.
            OSSleepTicks(OSMillisecondsToTicks(100));
        }
        IOS_Close(mcpFd);
    }
}

int main(int argc, char **argv) {
    WHBLogUdpInit();
    WHBLogPrintf("Hello from mocha");

    if (argc >= 1) {
        if (strncmp(argv[0], "fs:/", 4) == 0) {
            strncpy((char *) 0xF417FEF0, argv[0], 0xFF);
            DCStoreRange((void *) 0xF417FEF0, 0x100);
        }
    }
    uint64_t sysmenuIdUll = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_WII_U_MENU);
    memcpy((void *) 0xF417FFF0, &sysmenuIdUll, 8);
    DCStoreRange((void *) 0xF417FFF0, 0x8);

    StartMCPThreadIfMochaAlreadyRunning();

    ExecuteIOSExploit();

    // When the kernel exploit is set up successfully, we signal the ios to move on.
    int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
    if (mcpFd >= 0) {
        int in  = IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED;
        int out = 0;
        IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));

        in  = IPC_CUSTOM_START_MCP_THREAD;
        out = 0;
        IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));
        IOS_Close(mcpFd);
    }

    WHBLogPrintf("Bye from mocha");
    return 0;
}
