#include "ipc.h"

static int threadsStarted = 0;
extern void wupserver_init();

int _startMainThread(void) {
    if (threadsStarted == 0) {
        threadsStarted = 1;

#ifdef WUPSERVER_ENABLED
        wupserver_init();
#endif
        ipc_init();
    }
    return 0;
}