#include <cstring>

#include <coreinit/cache.h>
#include <coreinit/ios.h>
#include <sysapp/title.h>

#include <whb/log.h>
#include <whb/log_udp.h>

#include "common/ipc_defs.h"
#include "ios_exploit.h"

int main(int argc, char **argv) {
    WHBLogUdpInit();
    WHBLogPrintf("Hello from mocha");

    uint64_t sysmenuIdUll = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HOME_MENU);
    memcpy((void *) 0xF417FFF0, &sysmenuIdUll, 8);
    DCStoreRange((void *) 0xF417FFF0, 0x8);

    ExecuteIOSExploit();

    // When the kernel exploit is set up successfully, we signal the ios to move on.
    int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
    if (mcpFd >= 0) {
        int in = IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED;
        int out = 0;
        IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));

        in = IPC_CUSTOM_START_MCP_THREAD;
        out = 0;
        IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));
        IOS_Close(mcpFd);
    }

    WHBLogPrintf("Bye from mocha");
    return 0;
}
