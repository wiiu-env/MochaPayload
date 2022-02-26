/* Patch for MCP_LoadFile (ioctl 0x53).
 * Also adds a sibling ioctl, 0x64, that allows setting a custom path to a main RPX
 *
 * Reference for most of the types and whatever:
 * https://github.com/exjam/decaf-emu/tree/ios/src/libdecaf/src/ios/mcp
 *
 * This is a proof of concept, and shouldn't be used until it's cleaned up a bit.
 * Co-authored by exjam, Maschell and QuarkTheAwesome
 *
 * Flow of calls:
 * - kernel loads system libraries, app rpx or user calls OSDynLoad
 * - goes to loader.elf, which will call ioctl 0x53
 *   - with fileType = LOAD_FILE_CAFE_OS
 *   - on failure, again with LOAD_FILE_PROCESS_CODE
 *   - on failure, again with LOAD_FILE_SYS_DATA_CODE
 * - each request routes here where we can do whatever
 */

#include "../../common/ipc_defs.h"
#include "fsa.h"
#include "ipc_types.h"
#include "logger.h"
#include "svc.h"
#include <string.h>

int (*const real_MCP_LoadFile)(ipcmessage *msg)                                                                                                      = (void *) 0x0501CAA8 + 1; //+1 for thumb
int (*const MCP_DoLoadFile)(const char *path, const char *path2, void *outputBuffer, uint32_t outLength, uint32_t pos, int *bytesRead, uint32_t unk) = (void *) 0x05017248 + 1;

static int MCP_LoadCustomFile(int target, char *path, int filesize, int fileoffset, void *out_buffer, int buffer_len, int pos);

static bool replace_valid         = false;
static bool skipPPCSetup          = false;
static bool doWantReplaceRPX      = false;
static bool replace_target_device = 0;
static uint32_t rep_filesize      = 0;
static uint32_t rep_fileoffset    = 0;
static char rpxpath[256];

#define log(fmt, ...) log_printf("%s: " fmt, __FUNCTION__, __VA_ARGS__)
#define FAIL_ON(cond, val)         \
    if (cond) {                    \
        log(#cond " (%08X)", val); \
        return -29;                \
    }

int _MCP_LoadFile_patch(ipcmessage *msg) {

    FAIL_ON(!msg->ioctl.buffer_in, 0);
    FAIL_ON(msg->ioctl.length_in != 0x12D8, msg->ioctl.length_in);
    FAIL_ON(!msg->ioctl.buffer_io, 0);
    FAIL_ON(!msg->ioctl.length_io, 0);

    MCPLoadFileRequest *request = (MCPLoadFileRequest *) msg->ioctl.buffer_in;

    //dumpHex(request, sizeof(MCPLoadFileRequest));
    //DEBUG_FUNCTION_LINE("msg->ioctl.buffer_io = %p, msg->ioctl.length_io = 0x%X\n", msg->ioctl.buffer_io, msg->ioctl.length_io);
    //DEBUG_FUNCTION_LINE("request->type = %d, request->pos = %d, request->name = \"%s\"\n", request->type, request->pos, request->name);

    int replace_target     = replace_target_device;
    int replace_filesize   = rep_filesize;
    int replace_fileoffset = rep_fileoffset;
    char *replace_path     = rpxpath;

    if (strlen(request->name) > 1 && request->name[strlen(request->name) - 1] == 'x') {
        if (strncmp(request->name, "safe.rpx", strlen("safe.rpx")) != 0) {
            //DEBUG_FUNCTION_LINE("set replace_valid to false\n");
            replace_valid = false;
        } else if (request->pos == 0) {
            if (replace_valid) {
                //DEBUG_FUNCTION_LINE("set doWantReplaceRPX to true\n");
                doWantReplaceRPX = true;
            }
        }
    }
    if (strncmp(request->name, "men.rpx", strlen("men.rpx")) == 0) {
        replace_path = "wiiu/root.rpx";
        if (skipPPCSetup) {
            replace_path = "wiiu/men.rpx";
        }
        // At startup we want to hook into the Wii U Menu by replacing the men.rpx with a file from the SD Card
        // The replacement may restart the application to execute a kernel exploit.
        // The men.rpx is hooked until the "IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED" command is passed to IOCTL 0x100.
        // If the loading of the replacement file fails, the Wii U Menu is loaded normally.
        replace_target     = LOAD_FILE_TARGET_SD_CARD;
        replace_filesize   = 0; // unknown
        replace_fileoffset = 0;
    } else if (strncmp(request->name, "safe.rpx", strlen("safe.rpx")) == 0) {
        // if we don't explicitly replace files, we do want replace the Health and Safety app with the HBL
        if (request->pos == 0 && !doWantReplaceRPX) {
            replace_path   = "wiiu/apps/homebrew_launcher/homebrew_launcher.rpx";
            replace_target = LOAD_FILE_TARGET_SD_CARD;
            //doWantReplaceXML = false;
            doWantReplaceRPX   = true;
            replace_filesize   = 0; // unknown
            replace_fileoffset = 0;
        }
    } else if (!doWantReplaceRPX) {
        doWantReplaceRPX = false; // Only replace it once.
        replace_path     = NULL;
        return real_MCP_LoadFile(msg);
    }

    if (replace_path != NULL && strlen(replace_path) > 0) {
        doWantReplaceRPX = false; // Only replace it once.
        int result       = MCP_LoadCustomFile(replace_target, replace_path, replace_filesize, replace_fileoffset, msg->ioctl.buffer_io, msg->ioctl.length_io, request->pos);

        if (result >= 0) {
            return result;
        }
    } else {
        DEBUG_FUNCTION_LINE("replace_path was NULL\n");
    }

    return real_MCP_LoadFile(msg);
}


// Set filesize to 0 if unknown.
static int MCP_LoadCustomFile(int target, char *path, int filesize, int fileoffset, void *buffer_out, int buffer_len, int pos) {
    if (path == NULL || (filesize > 0 && (pos > filesize))) {
        return 0;
    }

    char filepath[256];
    memset(filepath, 0, sizeof(filepath));
    strncpy(filepath, path, sizeof(filepath) - 1);

    char mountpath[] = "/vol/storage_iosu_homebrew";

    if (target == LOAD_FILE_TARGET_SD_CARD) {
        int fsa_h     = svcOpen("/dev/fsa", 0);
        int mount_res = FSA_Mount(fsa_h, "/dev/sdcard01", mountpath, 2, NULL, 0);
        svcClose(fsa_h);

        strncpy(filepath, mountpath, sizeof(filepath) - 1);
        strncat(filepath, "/", (sizeof(filepath) - 1) - strlen(filepath));
        strncat(filepath, path, (sizeof(filepath) - 1) - strlen(filepath));
    }

    DEBUG_FUNCTION_LINE("Load custom path \"%s\"\n", filepath);

    int bytesRead = 0;
    int result    = MCP_DoLoadFile(filepath, NULL, buffer_out, buffer_len, pos + fileoffset, &bytesRead, 0);
    //log("MCP_DoLoadFile returned %d, bytesRead = %d pos %d \n", result, bytesRead, pos + fileoffset);

    if (result >= 0) {
        if (bytesRead <= 0) {
            result = 0;
        } else {
            if (filesize > 0 && (bytesRead + pos > filesize)) {
                result = filesize - pos;
            } else {
                result = bytesRead;
            }
        }
    }

    if (result < 0x400000 && target == LOAD_FILE_TARGET_SD_CARD) {
        int fsa_h       = svcOpen("/dev/fsa", 0);
        int unmount_res = FSA_Unmount(fsa_h, mountpath, 0x80000002);
        svcClose(fsa_h);
    }

    return result;
}

int _MCP_ReadCOSXml_patch(uint32_t u1, uint32_t u2, MCPPPrepareTitleInfo *xmlData) {
    int (*const real_MCP_ReadCOSXml_patch)(uint32_t u1, uint32_t u2, MCPPPrepareTitleInfo * xmlData) = (void *) 0x050024ec + 1; //+1 for thumb

    int res = real_MCP_ReadCOSXml_patch(u1, u2, xmlData);

    // Give us sd access!
    xmlData->permissions[4].mask = 0xFFFFFFFFFFFFFFFF;
    /*
    For some reason Mass Effects 3 softlocks when it has full ACP permissions.
    */
    if (xmlData->titleId != 0x000500001010DC00 && // Mass Effect 3 Special Edition USA
        xmlData->titleId != 0x000500001010F500 && // Mass Effect 3 Special Edition EUR
        xmlData->titleId != 0x0005000010113000) { // Mass Effect 3 Special Edition JPN

        // Give all titles permission to use ACP
        xmlData->permissions[10].mask = 0xFFFFFFFFFFFFFFFF;
    }

    // if we replace the RPX we want to increase the max_codesize and give us full permission!
    if (replace_valid) {
        if (xmlData->titleId == 0x000500101004E000 ||
            xmlData->titleId == 0x000500101004E100 ||
            xmlData->titleId == 0x000500101004E200) {
            xmlData->codegen_size = 0x02000000;
            xmlData->codegen_core = 0x80000001;
            xmlData->max_size     = 0x40000000;

            // Set maximum codesize to 64 MiB
            xmlData->max_codesize  = 0x04000000;
            xmlData->avail_size    = 0;
            xmlData->overlay_arena = 0;

            // Give us full permissions everywhere
            for (uint32_t i = 0; i < 19; i++) {
                xmlData->permissions[i].mask = 0xFFFFFFFFFFFFFFFF;
            }

            xmlData->default_stack0_size   = 0;
            xmlData->default_stack1_size   = 0;
            xmlData->default_stack2_size   = 0;
            xmlData->default_redzone0_size = 0;
            xmlData->default_redzone1_size = 0;
            xmlData->default_redzone2_size = 0;
            xmlData->exception_stack0_size = 0x00001000;
            xmlData->exception_stack1_size = 0x00001000;
            xmlData->exception_stack2_size = 0x00001000;
        }
    }

    // When the PPC Kernel reboots we replace the men.rpx to set up our PPC side again
    // for this the Wii U Menu temporarily gets replaced by our root.rpx and needs code gen access
    if (!skipPPCSetup) {
        if (xmlData->titleId == 0x0005001010040000 ||
            xmlData->titleId == 0x0005001010040100 ||
            xmlData->titleId == 0x0005001010040200) {

            xmlData->codegen_size = 0x02000000;
            xmlData->codegen_core = 0x80000001;
            xmlData->max_codesize = 0x02800000;
        }
    }

    return res;
}

extern int _startMainThread(void);

/*  RPX replacement! Call this ioctl to replace the next loaded RPX with an arbitrary path.
    DO NOT RETURN 0, this affects the codepaths back in the IOSU code */
int _MCP_ioctl100_patch(ipcmessage *msg) {
    /*  Give some method to detect this ioctl's prescence, even if the other args are bad */
    if (msg->ioctl.buffer_io && msg->ioctl.length_io >= sizeof(u32)) {
        *(u32 *) msg->ioctl.buffer_io = 1;
    }

    FAIL_ON(!msg->ioctl.buffer_in, 0);
    FAIL_ON(!msg->ioctl.length_in, 0);

    if (msg->ioctl.buffer_in && msg->ioctl.length_in >= 4) {
        int command = msg->ioctl.buffer_in[0];

        switch (command) {
            case IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED: {
                DEBUG_FUNCTION_LINE("IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED\n");
                skipPPCSetup = true;
                break;
            }
            case IPC_CUSTOM_META_XML_READ: {
                if (msg->ioctl.length_io >= sizeof(ACPMetaXml)) {
                    DEBUG_FUNCTION_LINE("IPC_CUSTOM_META_XML_READ\n");
                    ACPMetaXml *app_ptr = (ACPMetaXml *) msg->ioctl.buffer_io;
                    strncpy(app_ptr->longname_en, rpxpath, 256 - 1);
                    strncpy(app_ptr->shortname_en, rpxpath, 256 - 1);
                }
                break;
            }
            case IPC_CUSTOM_LOAD_CUSTOM_RPX: {
                DEBUG_FUNCTION_LINE("IPC_CUSTOM_LOAD_CUSTOM_RPX\n");

                if (msg->ioctl.length_in >= 0x110) {
                    int target     = msg->ioctl.buffer_in[0x04 / 0x04];
                    int filesize   = msg->ioctl.buffer_in[0x08 / 0x04];
                    int fileoffset = msg->ioctl.buffer_in[0x0C / 0x04];
                    char *str_ptr  = (char *) &msg->ioctl.buffer_in[0x10 / 0x04];
                    memset(rpxpath, 0, sizeof(rpxpath));

                    strncpy(rpxpath, str_ptr, 256 - 1);

                    rep_filesize     = filesize;
                    rep_fileoffset   = fileoffset;
                    doWantReplaceRPX = true;
                    //doWantReplaceXML = true;
                    replace_valid = true;

                    DEBUG_FUNCTION_LINE("Will load %s for next title from target: %d (offset %d, filesize %d)\n", rpxpath, target, rep_fileoffset, rep_filesize);
                }
                break;
            }
            case IPC_CUSTOM_START_MCP_THREAD: {
                _startMainThread();
                break;
            }
            case IPC_CUSTOM_COPY_ENVIRONMENT_PATH: {
                if (msg->ioctl.buffer_io && msg->ioctl.length_io >= 0x100) {
                    strncpy((char *) msg->ioctl.buffer_io, (void *) 0x05119F00, 0xFF);
                    return 0;
                } else {
                    return 29;
                }
            }
            case IPC_CUSTOM_START_USB_LOGGING: {
                if (*((uint32_t *) 0x050290dc) == 0x42424242) {
                    // Skip syslog after a reload
                    break;
                }
                int handle = svcOpen("/dev/testproc1", 0);
                if (handle > 0) {
                    svcResume(handle);
                    svcClose(handle);
                }

                handle = svcOpen("/dev/usb_syslog", 0);
                if (handle > 0) {
                    svcResume(handle);
                    svcClose(handle);
                }

                // Kill existing syslogs to avoid long catch up
                uint32_t *bufferPtr = (uint32_t *) (*(uint32_t *) 0x05095ecc);
                bufferPtr[0]        = 0;
                bufferPtr[1]        = 0;

                break;
            }
            default: {
            }
        }
    } else {
        return 29;
    }
    return 0;
}
