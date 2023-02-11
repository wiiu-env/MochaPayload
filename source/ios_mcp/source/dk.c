#include "dk.h"
#include "imports.h"
#include "smd.h"
#include "socket.h"
#include "svc.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG_LOGGING

#define DK_PORT            3000
#define DK_THREAD_INTERVAL 600

#define DK_SMD_IDX_READ    0
#define DK_SMD_IDX_WRITE   1

static u8 threadStack[0x500] __attribute__((aligned(0x20)));
static int threadRunning = 0;
static int threadId      = 0;
static int servfd        = 0;

static int setupServer(int port) {
    if (socketInit() < 0) {
        printf("DK: failed to init socketlib\n");
        smdIopClose(DK_SMD_IDX_READ);
        smdIopClose(DK_SMD_IDX_WRITE);
        return -1;
    }

    printf("DK: socket init\n");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("DK: failed to create socket\n");
        smdIopClose(DK_SMD_IDX_READ);
        smdIopClose(DK_SMD_IDX_WRITE);
        return -1;
    }

    printf("DK: socket created\n");

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("DK warning: failed to set SO_REUSEADDR\n");
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port        = port;
    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
        printf("DK: failed to bind socket\n");
        goto quit;
    }

    printf("DK: socket bound\n");

    return sockfd;

quit:;
    closesocket(sockfd);
    return -1;
}

static int handleServer(int connfd) {
    int nonblock = 1;
    if (setsockopt(connfd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock)) < 0) {
        printf("DK: failed to make connection non-blocking\n");
        return 0;
    }

    int sack = 1;
    if (setsockopt(connfd, SOL_SOCKET, SO_TCPSACK, &sack, sizeof(sack)) < 0) {
        printf("DK: failed to set SO_TCPSACK\n");
        return 0;
    }

    int nodelay = 1;
    if (setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        printf("DK: failed to set TCP_NODELAY\n");
        return 0;
    }

    // create a message queue and timer
    uint32_t messageBuf;
    int queueId = IOS_CreateMessageQueue(&messageBuf, 1);
    if (queueId < 0) {
        printf("DK: failed to create message queue\n");
        return -1;
    }

    int timerId = IOS_CreateTimer(0, 0, queueId, 0xdeafcafe);
    if (timerId < 0) {
        printf("DK: failed to create timer\n");
        IOS_DestroyMessageQueue(queueId);
        return -1;
    }

    while (threadRunning) {
        // restart timer
        if (IOS_RestartTimer(timerId, DK_THREAD_INTERVAL, 0)) {
            printf("DK: failed to restart timer\n");
            break;
        }

        // wait until the timer sends a message
        uint32_t message;
        if (IOS_ReceiveMessage(queueId, &message, 0) < 0) {
            printf("DK: failed to receive message\n");
            break;
        }

        SmdInterfaceState readState;
        if (smdIopGetInterfaceState(DK_SMD_IDX_READ, NULL, &readState) != 0) {
            printf("DK: Failed to get interface state, closing...\n");
            break;
        }

        SmdInterfaceState writeState;
        if (smdIopGetInterfaceState(DK_SMD_IDX_WRITE, NULL, &writeState) != 0) {
            printf("DK: Failed to get interface state, closing...\n");
            break;
        }

        if (readState != SMD_INTERFACE_STATE_OPENED || writeState != SMD_INTERFACE_STATE_OPENED) {
            printf("DK: Closing interfaces\n");
            break;
        }

        // process up to 100 writes before waiting
        for (int i = 0; i < 100; ++i) {
            SmdReceiveData receiveData;
            if (smdIopReceive(DK_SMD_IDX_WRITE, &receiveData) != 0) {
                break;
            }

#ifdef DEBUG_LOGGING
            printf("DK: received write vector\n");
#endif

            if (receiveData.type != SMD_ELEMENT_TYPE_VECTOR) {
                printf("DK Warn: Received smd type %ld on write queue\n", receiveData.type);
                continue;
            }

            if (receiveData.vector->id != DK_PCHAR_COMMAND_WRITE) {
                printf("DK Warn: Received command %lx on write queue\n", receiveData.vector->id);
                smdIopSendVector(DK_SMD_IDX_WRITE, receiveData.vector);
                continue;
            }

            DKPCHARResult *result     = receiveData.vector->vecs[0].ptr;
            DKPCHARResponse *response = receiveData.vector->vecs[2].ptr;

            response->buf = receiveData.vector->vecs[1].ptr;

            // send data
            int32_t error    = 0;
            uint32_t to_send = receiveData.vector->vecs[1].size;

#ifdef DEBUG_LOGGING
            printf("DK: sending %lu bytes\n", to_send);
#endif

            uint32_t offset = 0;
            while (to_send > 0) {
                int res = send(connfd, receiveData.vector->vecs[1].ptr + offset, to_send, 0);
                if (res < 0) {
                    error = res;
                    break;
                }

                to_send -= res;
                offset += res;
            }

            result->error = error;
            smdIopSendVector(DK_SMD_IDX_WRITE, receiveData.vector);
        }

        // read pending data, if we have any
        for (int i = 0; i < 100; ++i) {
            char buf[500];
            int32_t res = recv(connfd, buf, sizeof(buf), 0);
            if (res == -0xafffa /*EWOULDBLOCK*/) {
                // no more data to receive
                break;
            }

            if (res < 0) {
                printf("DK: recv returned %lx\n", res);
                break;
            }

            if (res == 0) {
                printf("DK: client disconnected\n");
                IOS_DestroyTimer(timerId);
                IOS_DestroyMessageQueue(queueId);
                return 0;
            }

#ifdef DEBUG_LOGGING
            printf("DK: recv: %ld\n", res);
#endif

            SmdReceiveData receiveData;
            if (smdIopReceive(DK_SMD_IDX_READ, &receiveData) != 0) {
                printf("DK Warn: no vector in read queue, dropping packet\n");
                break;
            }

#ifdef DEBUG_LOGGING
            printf("DK: received read vector\n");
#endif

            if (receiveData.type != SMD_ELEMENT_TYPE_VECTOR) {
                printf("DK Warn: Received smd type %ld on read queue\n", receiveData.type);
                continue;
            }

            if (receiveData.vector->id != DK_PCHAR_COMMAND_READ) {
                printf("DK Warn: Received command %lx on read queue\n", receiveData.vector->id);
                smdIopSendVector(DK_SMD_IDX_READ, receiveData.vector);
                continue;
            }

            DKPCHARResult *result     = receiveData.vector->vecs[0].ptr;
            DKPCHARResponse *response = receiveData.vector->vecs[2].ptr;

            response->buf = receiveData.vector->vecs[1].ptr;

            if (receiveData.vector->vecs[1].size < (uint32_t) res) {
                printf("DK: Warning truncating received data\n");
                res = receiveData.vector->vecs[1].size;
            }

            if (res > 0) {
                memcpy(receiveData.vector->vecs[1].ptr, buf, res);
                result->error  = 0;
                result->param0 = res;
            } else {
                result->error = res;
            }

            smdIopSendVector(DK_SMD_IDX_READ, receiveData.vector);
        }
    }

    IOS_DestroyTimer(timerId);
    IOS_DestroyMessageQueue(queueId);

    return -1;
}

static int dkThread(void *arg) {
    printf("DK: thread started\n");
    threadRunning = 1;

    servfd = setupServer(DK_PORT);
    if (servfd < 0) {
        goto exit;
    }

    while (threadRunning) {
        if (listen(servfd, 1) != 0) {
            printf("DK: failed listening for connections\n");
            goto exit;
        }

        printf("DK: listening for connections on port %d\n", DK_PORT);

        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int connfd    = accept(servfd, (struct sockaddr *) &clientaddr, &len);
        if (connfd < 0) {
            printf("DK: failed to accept connection\n");
            continue;
        }

        uint8_t *addr = (uint8_t *) &clientaddr.sin_addr.s_addr;
        printf("DK: Connected to %d.%d.%d.%d\n", addr[0], addr[1], addr[2], addr[3]);

        if (handleServer(connfd) < 0) {
            closesocket(connfd);
            break;
        }

        closesocket(connfd);
    }

exit:;
    printf("DK: exited thread\n");
    if (servfd >= 0) {
        closesocket(servfd);
        servfd = -1;
    }

    // close smd
    smdIopClose(DK_SMD_IDX_READ);
    smdIopClose(DK_SMD_IDX_WRITE);

    threadRunning = 0;
    return 0;
}

static void dkPCHARDeactivateSmd() {
    // close server socket and tell thread to stop
    if (servfd >= 0) {
        closesocket(servfd);
        servfd = -1;
    }

    threadRunning = 0;

    // wait for thread to return
    IOS_JoinThread(threadId, NULL);
}

int dkPCHARActivateSmd(IOSVec *vecs) {
    int32_t ret                   = 0;
    DKPCHARResult *result         = (DKPCHARResult *) vecs[0].ptr;
    DKPCHARActivateParams *params = (DKPCHARActivateParams *) vecs[1].ptr;

    printf("PCHAR Activate (%lx) for %s: SMDs %s %s\n", params->command, params->path, params->smdReadName, params->smdWriteName);

    // Prevent starting another thread while the old one is still running
    if (threadRunning) {
        printf("DK: thread is already running, stopping...\n");
        dkPCHARDeactivateSmd();
    }

    ret = smdIopInit(DK_SMD_IDX_WRITE, (SmdControlTable *) vecs[2].ptr, params->smdWriteName, 0);
    if (ret != 0) {
        goto done;
    }

    ret = smdIopInit(DK_SMD_IDX_READ, (SmdControlTable *) vecs[3].ptr, params->smdReadName, 0);
    if (ret != 0) {
        goto done;
    }

    ret = smdIopOpen(DK_SMD_IDX_WRITE);
    if (ret != 0) {
        goto done;
    }

    ret = smdIopOpen(DK_SMD_IDX_READ);
    if (ret != 0) {
        goto done;
    }

    threadId = svcCreateThread(dkThread, 0, (u32 *) (threadStack + sizeof(threadStack)), sizeof(threadStack), 0x77, 0);
    if (threadId < 0) {
        ret = threadId;
        goto done;
    }

    if (svcStartThread(threadId) < 0) {
        printf("DK: failed to start thread\n");
        ret = -11;
    }

done:;
    printf("PCHAR Activate %lx\n", ret);
    result->error = ret;
    return ret;
}
