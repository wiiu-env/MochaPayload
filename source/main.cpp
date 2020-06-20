#include <stdio.h>
#include <string.h>
#include <string>

#include <coreinit/cache.h>

#include <coreinit/ios.h>


#include "whb/log.h"
#include "whb/log_udp.h"
#include "ios_exploit.h"

extern "C" uint64_t _SYSGetSystemApplicationTitleId(int);

int main(int argc, char **argv) {
    WHBLogUdpInit();
    WHBLogPrintf("Hello from mocha");
    unsigned long long sysmenuIdUll = _SYSGetSystemApplicationTitleId(0);

    memcpy((void *) 0xF417FFF0, &sysmenuIdUll, 8);
    DCStoreRange((void *) 0xF417FFF0, 0x8);

    ExecuteIOSExploit();

    // When the kernel exploit is set up successfully, we signal the ios to move on.
    int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
    if (mcpFd >= 0) {
        int in = 0xFD;//IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED;
        int out = 0;

        IOS_Ioctl(mcpFd, 100, &in, sizeof(in), &out, sizeof(out));
        IOS_Close(mcpFd);
    }

    WHBLogPrintf("Bye from mocha");
    return 0;
}
