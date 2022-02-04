#include "ipc.h"
#include "wupserver.h"

static int threadsStarted = 0;

int _startMainThread(void) {
    if (threadsStarted == 0) {
        threadsStarted = 1;

        wupserver_init();
        ipc_init();
    }
    return 0;
}