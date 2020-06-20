#include <stdio.h>
#include <string.h>
#include <string>

#include <coreinit/cache.h>


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
    WHBLogPrintf("Bye from mocha");
    return 0;
}
