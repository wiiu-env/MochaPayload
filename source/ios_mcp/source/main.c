#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "wupserver.h"
#include "ipc.h"
#include "svc.h"
#include "text.h"
#include "../../common/kernel_commands.h"

static int threadsStarted = 0;

int _startMainThread(void) {
    if (threadsStarted == 0) {
        threadsStarted = 1;

        wupserver_init();
        ipc_init();
    }
    return 0;
}