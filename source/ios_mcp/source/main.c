#include "ipc.h"

static int threadsStarted = 0;
extern void wupserver_init();

int _startMainThread(void) {
    if (threadsStarted == 0) {
        threadsStarted = 1;

        wupserver_init();
        ipc_init();
    }
    return 0;
}