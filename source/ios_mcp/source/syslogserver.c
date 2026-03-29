#include "syslogserver.h"
#include "../../common/kernel_commands.h"
#include "imports.h"
#include "logger.h"
#include "socket.h"
#include "svc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG_LOGGING

#define UDP_BEACON_PORT       4445
#define SHELL_PORT            7965
#define SHELL_THREAD_INTERVAL (10 * 1000)
#define WATCHDOG_MAX_TICKS    (5 * 1000 * 1000 / SHELL_THREAD_INTERVAL)
#define EXPECTED_UDP_BEACON   "WIIU_TCP_SYSLOG_BEACON_V1"

static uint8_t sTCPSyslogThreadStack[0x700] __attribute__((aligned(0x20)));
static int sTCPSyslogThreadRunning = 0;
static int sTCPSyslogThreadId      = 0;
static int sTCPSyslogSocket        = 0;
static int sUDPSyslogSocket        = 0;
static int sFilterEnabled          = 0;
static in_addr_t sAllowedIP        = 0;

typedef void (*ReplayCallback)(char *data, int size, uint32_t flag);

void (*const Syslog_SignalSyslogOutputThread)()                                                                                                         = (void *) 0x05029104 + 1; //+1 for thumb
void (*const Syslog_FlushHistoryToOutput)(const SyslogCircularBuffer *buffer, uint32_t flags)                                                           = (void *) 0x05110744 + 1; //+1 for thumb
void (*const CircularBuffer_ReplayAll)(SyslogCircularBuffer *circBuffer, char *tmpBuffer, uint chunkSize, ReplayCallback callback, uint32_t outputFlag) = (void *) 0x0505977c;

static void *allocIobuf(int size) {
    void *ptr = svcAllocAlign(0xCAFF, size, 0x40);

    memset(ptr, 0x00, size);

    return ptr;
}

static void freeIobuf(void *ptr) {
    svcFree(0xCAFF, ptr);
}


size_t CircularBuffer_FreeSpace(const SyslogCircularBuffer *buffer) {
    size_t rawDiff     = (buffer->capacity + buffer->readOffset) - buffer->writeOffset;
    size_t gapAdjusted = rawDiff - 1;
    size_t freeSpace   = gapAdjusted % buffer->capacity;
    return freeSpace;
}

SyslogCircularBuffer *sTCPSyslogBuffer = 0;
int sTCPSyslogBufferSemaphore          = 0;

void CustomBuffer_ForceWrite_Safe(char *buffer, const uint32_t len) {
    IOS_WaitSemaphore(sTCPSyslogBufferSemaphore, 0);
    Syslog_CircularBuffer_ForceWrite(sTCPSyslogBuffer, buffer, len);
    IOS_SignalSemaphore(sTCPSyslogBufferSemaphore);
}

uint32_t CustomBuffer_Read_Safe(char *buffer, const uint32_t buffer_len, uint32_t *read_ptr) {
    IOS_WaitSemaphore(sTCPSyslogBufferSemaphore, 0);
    *read_ptr          = sTCPSyslogBuffer->readOffset;
    const uint32_t res = Syslog_CircularBuffer_Read(sTCPSyslogBuffer, buffer, buffer_len);
    IOS_SignalSemaphore(sTCPSyslogBufferSemaphore);
    return res;
}

void CustomBuffer_ResetReadOffset_Safe(const uint32_t read_offset) {
    IOS_WaitSemaphore(sTCPSyslogBufferSemaphore, 0);
    sTCPSyslogBuffer->readOffset = read_offset;
    IOS_SignalSemaphore(sTCPSyslogBufferSemaphore);
}


void WriteToTCPBuffer(char *data, int len, uint32_t flag) {
    (void) flag;
    if (sTCPSyslogBuffer == NULL || sTCPSyslogBufferSemaphore == 0) {
        return;
    }

    // This will always write the full data into the buffer. It may override data that has not yet send to the user
    CustomBuffer_ForceWrite_Safe(data, len);
}

void Syslog_FlushHistoryToTCP(SyslogCircularBuffer *buffer) {
    char *tmpBuffer = allocIobuf(500);
    if (tmpBuffer != (char *) 0x0) {
        CircularBuffer_ReplayAll(buffer, tmpBuffer, 500, WriteToTCPBuffer, SYSLOG_BUFFER_FLAG_TCP_SERVER);
        freeIobuf(tmpBuffer);
    }
}

bool sHasSkippedFlushOutputForUSB   = false;
bool gSkipSyslogCatchUpForUSBOutput = false;

void _Syslog_FlushHistoryToOutputForUSB(SyslogCircularBuffer *buffer, const uint32_t flags) {
    if (gSkipSyslogCatchUpForUSBOutput && !sHasSkippedFlushOutputForUSB) {
        Syslog_FlushHistoryToTCP(gActiveSyslogBuffer);
        gActiveSyslogBuffer->readOffset = gActiveSyslogBuffer->writeOffset;
        sHasSkippedFlushOutputForUSB    = true;
        return;
    }

    Syslog_FlushHistoryToOutput(buffer, flags);
}

void _Syslog_RouteOutputPatch(char *buffer, uint32_t len, uint32_t flags) {
    int (*const real_Syslog_RouteOutput)(char *, uint32_t, uint32_t) = (void *) 0x051107a0 + 1; //+1 for thumb
    real_Syslog_RouteOutput(buffer, len, flags);

    if (sTCPSyslogBuffer == NULL || sTCPSyslogBufferSemaphore == 0) {
        return;
    }

    WriteToTCPBuffer(buffer, len, SYSLOG_BUFFER_FLAG_TCP_SERVER);
}

// --- Discovery ---
static int sTCPSyslogFindServer(struct sockaddr_in *server_addr) {
    char *buffer = NULL;
    int udp_sock = -1;
    int ret      = -1;
    udp_sock     = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        // DEBUG_FUNCTION_LINE("Failed to create UDP socket\n");
        goto exit;
    }
    sUDPSyslogSocket = udp_sock;

    struct sockaddr_in addr = {0};
    addr.sin_family         = AF_INET;
    addr.sin_port           = UDP_BEACON_PORT;
    addr.sin_addr.s_addr    = INADDR_ANY;

    int one = 1;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (bind(udp_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        DEBUG_FUNCTION_LINE("Failed to bind udp socket\n");
        goto exit;
    }

#define BUFFER_SIZE 64
    buffer = allocIobuf(BUFFER_SIZE);
    if (!buffer) {
        goto exit;
    }
    struct sockaddr_in sender;
    socklen_t senderLen = sizeof(sender);

#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Listening for beacon\n");
#endif

    // Loop until we find a VALID server or a fatal error occurs
    while (1) {
        // Reset buffer for safety
        memset(buffer, 0, BUFFER_SIZE);

        // Block until we hear SOMETHING
        int len = recvfrom(udp_sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *) &sender, &senderLen);

        if (len < 0) {
            DEBUG_FUNCTION_LINE("Error in recvfrom\n");
            // Error in recvfrom
            ret = -1;
            break;
        }

        // 1. Check if the beacon text is correct
        if (strncmp(EXPECTED_UDP_BEACON, buffer, len) != 0) {
            DEBUG_FUNCTION_LINE("Received unexpected beacon: %s. Sleeping for 5 seconds and trying again\n", buffer);
            ret = -2;
            break;
        }

        // 2. Check IP Filter (If enabled)
        if (sFilterEnabled) {
            if (sender.sin_addr.s_addr != sAllowedIP) {
                // IP mismatch - Ignore this packet and keep listening
                const unsigned char *bytes = (const unsigned char *) &sender.sin_addr;
                DEBUG_FUNCTION_LINE("Ignored beacon from unauthorized IP: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
                ret = -2;
                break;
            }
        }

        // If we got here: Beacon is valid AND IP is allowed (or filter is off)
        memset(server_addr, 0, sizeof(*server_addr));
        server_addr->sin_family = AF_INET;
        server_addr->sin_port   = SHELL_PORT;
        server_addr->sin_addr   = sender.sin_addr;

#ifdef DEBUG_LOGGING
        const unsigned char *bytes = (const unsigned char *) &sender.sin_addr;
        DEBUG_FUNCTION_LINE("Received beacon from authorized IP: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
#endif

        ret = 0; // Success
        break;
    }

exit:
    if (buffer) freeIobuf(buffer);
    if (udp_sock >= 0) {
        closesocket(udp_sock);
    }
    sUDPSyslogSocket = -1;

    return ret;
}


static void TCPSyslogKillConnection() {
    if (sUDPSyslogSocket >= 0) {
        closesocket(sUDPSyslogSocket);
        usleep(10 * 1000);
    }
    sUDPSyslogSocket = -1;
    if (sTCPSyslogSocket > 0) {
        closesocket(sTCPSyslogSocket);
        usleep(10 * 1000);
    }
    sTCPSyslogSocket = -1;
}

void sTCPSyslog_Commands_Help(void);

void sTCPSyslog_Commands_Reboot(void) {
    TCPSyslogKillConnection();
    IOS_Shutdown(1);
}
void sTCPSyslog_Commands_Shutdown(void) {
    TCPSyslogKillConnection();
    IOS_Shutdown(0);
}

int (*const shellCommand_title_launch)(int argc, char **argv) = (void *) (0x0510c9a0 | 1);
void sTCPSyslog_Commands_Reload(void) {
    //TCPSyslogKillConnection();
    // This triggers a full cafe relaunch into the specified title
    // not entirely sure how shell commands handle these args but this seems to work
    int argc     = 2;
    char *argv[] = {
            "",
            "",
            (char *) (uint32_t) (gCurrentColdbootTitle >> 32),
            (char *) (uint32_t) (gCurrentColdbootTitle & 0xffffffff),
    };
    shellCommand_title_launch(argc, argv);
}

typedef struct SyslogCustomIOSUCommands {
    const char *name;
    void (*func)(void);
    const char *description;
} SyslogCustomIOSUCommands;

// 2. Updated Command Table
static const SyslogCustomIOSUCommands gCommands[] = {
        {"reboot",
         sTCPSyslog_Commands_Reboot,
         "Restarts the system immediately."},
        {"shutdown",
         sTCPSyslog_Commands_Shutdown,
         "Shuts the system down immediately."},
        {"reload",
         sTCPSyslog_Commands_Reload,
         "Force reloads the iosu"},
        {"help",
         sTCPSyslog_Commands_Help,
         "Displays this list of available commands."}};

#define COMMAND_COUNT (sizeof(gCommands) / sizeof(gCommands[0]))

// 3. Dynamic Help Generation
void sTCPSyslog_Commands_Help(void) {
    DEBUG_FUNCTION_LINE("\n--- IOSU Help Menu ---\n");
    DEBUG_FUNCTION_LINE("%-10s | %s\n", "Command", "Description");
    DEBUG_FUNCTION_LINE("--------------------------------------------\n");

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        DEBUG_FUNCTION_LINE("%-10s - %s\n", gCommands[i].name, gCommands[i].description);
    }
    DEBUG_FUNCTION_LINE("\n");
}

void sTCPSyslog_ProcessIOSUCommands(char *input) {
    const char *prefix = "iosu ";
    if (strncmp(input, prefix, 5) != 0) return;

    char *cmd_name = input + strlen(prefix);

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strncmp(cmd_name, gCommands[i].name, strlen(gCommands[i].name)) == 0) {
            if (gCommands[i].func) {
                gCommands[i].func();
            } else {
                DEBUG_FUNCTION_LINE("Command '%s' is defined but not implemented.\n", cmd_name);
            }
            return;
        }
    }
    DEBUG_FUNCTION_LINE("Unknown command. Type 'iosu help' for a list.\n");
}


static int sTCPSyslogHandleServer() {
    struct sockaddr_in server_addr;
#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Wait for beacon\n");
#endif

    while (socketInit() <= 0) {
#ifdef DEBUG_LOGGING
        DEBUG_FUNCTION_LINE("opening /dev/socket...");
#endif
        usleep(1000 * 100);
    }

    // 1. Wait for PC Beacon
    if (sTCPSyslogFindServer(&server_addr) < 0) {
        // Sleep 5 seconds then try again
        usleep(1 * 1000 * 1000);
        return 0;
    }

#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Found server! Lets try to connect to it \n");
#endif
    // 2. Connect TCP
    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        return 0;
    }
    sTCPSyslogSocket = connfd;

    // 2. Connect to server
    if (connect(connfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        DEBUG_FUNCTION_LINE("Connection failed\n");
        closesocket(connfd);
        return 0;
    }

    DEBUG_FUNCTION_LINE("Connected to TCP Syslog server\n");

    // 4. Setup Non-Blocking IO
    int nonblock = 1;
    setsockopt(connfd, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));
    //int nodelay = 1;
    //setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

#define CMD_BUF_SIZE       0x100
#define MESSAGE_QUEUE_SIZE 10

    uint32_t qBuf[MESSAGE_QUEUE_SIZE];
    int queueId    = -1;
    int ioshHandle = -1;
    int timerId    = -1;
    char *cmdBuf   = NULL;

    queueId = IOS_CreateMessageQueue(qBuf, MESSAGE_QUEUE_SIZE);
    if (queueId < 0) {
        DEBUG_FUNCTION_LINE("Can't create message queue.\n");
        goto exit;
    }

    timerId = IOS_CreateTimer(0, 0, queueId, 0xdeafcafe);

    // Open /dev/iopsh for command injection
    ioshHandle = svcOpen("/dev/iopsh", 5);
    if (!ioshHandle) {
        DEBUG_FUNCTION_LINE("svcOpen(\"dev/iopsh\") failed.\n");
        goto exit;
    }
    cmdBuf = allocIobuf(CMD_BUF_SIZE);
    if (!cmdBuf) {
        DEBUG_FUNCTION_LINE("allocIobuf(0x%08X) failed.\n", CMD_BUF_SIZE);
        goto exit;
    }

    // Kickstart timer
    IOS_RestartTimer(timerId, SHELL_THREAD_INTERVAL, 0);

    // Let /dev/syslog know something is connected
    gSyslogBufferFlags |= SYSLOG_BUFFER_FLAG_TCP_SERVER;

    // trigger the semaphore to get our logs
    Syslog_SignalSyslogOutputThread();

    int watchdogTimer = WATCHDOG_MAX_TICKS; // Initialize watchdog

    while (sTCPSyslogThreadRunning) {
        uint32_t message = 0;

        // Wait for Timer
        if (IOS_ReceiveMessage(queueId, &message, 0) < 0) break;

        IOS_RestartTimer(timerId, SHELL_THREAD_INTERVAL, 0); // Schedule next tick

        // 1. Tick the Watchdog
        watchdogTimer--;
        if (watchdogTimer <= 0) {
            DEBUG_FUNCTION_LINE("Connection timed out (No Heartbeat)\n");
            break;
        }

        int res = recv(connfd, cmdBuf, CMD_BUF_SIZE - 1, 0);
        if (res != -0xafffa) { // EWOULDBLOCK
            if (res <= 0) {
                DEBUG_FUNCTION_LINE("recv failed %d\n", res);
                break;
            }
            watchdogTimer = WATCHDOG_MAX_TICKS;

            // 3. Handle Heartbeats vs Real Data
            // If we get just a null byte, it's a heartbeat. Ignore it.
            if (res == 1 && cmdBuf[0] == 0x00) {
                continue;
            }

            cmdBuf[res] = 0;

            sTCPSyslog_ProcessIOSUCommands(cmdBuf);

            // Inject command
            cmdBuf[res] = 0;
            if (ioshHandle > 0 && cmdBuf) {
                int size = res > CMD_BUF_SIZE ? CMD_BUF_SIZE : res;
                int ret  = svcIoctl(ioshHandle, 9, cmdBuf, size, 0, 0);
                if (ret < 0) {
                    DEBUG_FUNCTION_LINE("Injecting command failed: %d\n", res);
                }
            }
        }

        bool breakLoop = false;
        // always try to send logs
        while (true) {
            uint32_t latest_read_offset = 0;
            uint32_t readDataSize       = CustomBuffer_Read_Safe(cmdBuf, CMD_BUF_SIZE - 1, &latest_read_offset);
            if (readDataSize == 0) {
                break;
            }
            uint32_t totalSent = 0;
            int fails          = 0;
            while (totalSent < readDataSize) {
                int sent = send(connfd, cmdBuf + totalSent, readDataSize - totalSent, 0);
                if (sent < 0) {
                    if (sent == -0xafffa) { // EWOULDBLOCK
                        fails++;
                        if (fails > 10) {
                            breakLoop = true;
                            break;
                        }
                        continue;
                    }
                    breakLoop = true;
                    break; // Error
                }
                totalSent += sent;
            }
            if (breakLoop) {
                // readset read ptr
                CustomBuffer_ResetReadOffset_Safe((latest_read_offset + totalSent) % sTCPSyslogBuffer->capacity);
                break;
            }
        }
    }

exit:
    // On exit reset buffer
    //sTCPSyslogBuffer->readOffset = 0;
    //sTCPSyslogBuffer->writeOffset = 0;

    if (timerId != -1) {
        IOS_DestroyTimer(timerId);
    }
    timerId = -1;
    if (queueId != -1) {
        IOS_DestroyMessageQueue(queueId);
    }
    queueId = -1;
    if (cmdBuf) {
        freeIobuf(cmdBuf);
    }
    if (ioshHandle > 0) {
        svcClose(ioshHandle);
    }
    // mark as disconnected
    gSyslogBufferFlags &= ~(SYSLOG_BUFFER_FLAG_TCP_SERVER);
    closesocket(connfd);
    sTCPSyslogSocket = 0;

    DEBUG_FUNCTION_LINE("Disconnected from TCP Syslog server\n");
    return 0;
}

static int sTCPSyslogThread(void *context) {
    (void) context;
#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Thread started\n");
#endif
    sTCPSyslogThreadRunning = 1;


    while (sTCPSyslogThreadRunning) {
#ifdef DEBUG_LOGGING
        DEBUG_FUNCTION_LINE("Handle server connections\n");
#endif
        if (sTCPSyslogHandleServer() < 0) {
            break;
        }
    }

#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Exited thread\n");
#endif


    sTCPSyslogThreadRunning = 0;
    return 0;
}

uint32_t gSyslogTestWriteHeadIndex = 0;
char gSyslogTestSavedByteWriteHead = 0;
char gSyslogTestSavedByteIndexZero = 0;

int TCPSyslogStartThread(bool useIPFilter, uint32_t allowedIP) {
    if (sTCPSyslogThreadRunning) {
        if (sFilterEnabled == useIPFilter) {
            if (sAllowedIP == allowedIP) {
#ifdef DEBUG_LOGGING
                DEBUG_FUNCTION_LINE("TCP Server with same config already running, lets keep it running.\n");
#endif
                return 0;
            }
        }
#ifdef DEBUG_LOGGING
        DEBUG_FUNCTION_LINE("TCP Server is running with different config, we need to restart it\n");
#endif
        TCPSyslogEndThread();
        usleep(500 * 1000);
    }

    int ret                 = 0;
    sTCPSyslogThreadRunning = 0;
    sTCPSyslogThreadId      = -1;
    sUDPSyslogSocket        = -1;
    sTCPSyslogSocket        = -1;
    sFilterEnabled          = useIPFilter;
    sAllowedIP              = allowedIP;

    if (sTCPSyslogBufferSemaphore <= 0) {
        // Create semaphore to avoid access conflicts to the buffer.
        sTCPSyslogBufferSemaphore = IOS_CreateSemaphore(1, 1);
    }
    if (sTCPSyslogBuffer == NULL) {
        // The Wii U utilizes a dual-buffer system for syslogs, where each buffer is 0x40000 bytes.
        // The system alternates which buffer is active on every (warm) boot.
        //
        // From my observation the "old" buffer will be unused, at least at this point in time.
        // Therefore, we can safely repurpose it here as a temporary buffer for logs that need to be sent over TCP.
        if (gSyslogBuffers->header.activeEntry == 0) {
            sTCPSyslogBuffer = &gSyslogBuffers->entries[1].buffer;
#ifdef DEBUG_LOGGING
            DEBUG_FUNCTION_LINE("Using buffer 1\n");
#endif
        } else {
            sTCPSyslogBuffer = &gSyslogBuffers->entries[0].buffer;
#ifdef DEBUG_LOGGING
            DEBUG_FUNCTION_LINE("Using buffer 0\n");
#endif
        }

        // Make sure the buffer has the expected size.
        if (sTCPSyslogBuffer->capacity != 0x40000) {
            DEBUG_FUNCTION_LINE("TCPSyslog: Buffer has not the expected size 0x%lu bytes.\n", sTCPSyslogBuffer->capacity);
            sTCPSyslogBuffer = NULL;
        } else {
            // if any syslog client is connected, we need to flush the history to the tpc syslog buffer
            if ((gSyslogBufferFlags != 0)) {
                Syslog_FlushHistoryToTCP(gActiveSyslogBuffer);
            }
        }
    }

#ifdef DEBUG_LOGGING
    DEBUG_FUNCTION_LINE("Creating thread with sFilterEnabled = %d and sAllowedIP = %08lX\n", sFilterEnabled, (uint32_t) sAllowedIP);
#endif
    sTCPSyslogThreadId = svcCreateThread(sTCPSyslogThread, NULL, (u32 *) (sTCPSyslogThreadStack + sizeof(sTCPSyslogThreadStack)), sizeof(sTCPSyslogThreadStack), 0x77, 0);
    if (sTCPSyslogThreadId < 0) {
        ret = sTCPSyslogThreadId;
        goto done;
    }

    if (svcStartThread(sTCPSyslogThreadId) < 0) {
        DEBUG_FUNCTION_LINE("Failed to start thread\n");
        ret = -11;
    }

done:;
    return ret;
}

void TCPSyslogEndThread() {
    sTCPSyslogThreadRunning = 0;

    TCPSyslogKillConnection();

    if (sTCPSyslogThreadId > 0) {
        IOS_JoinThread(sTCPSyslogThreadId, NULL);
    }
}