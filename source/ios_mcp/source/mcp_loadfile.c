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
#include "../../common/kernel_commands.h"
#include "fsa.h"
#include "imports.h"
#include "ipc_types.h"
#include "logger.h"
#include "svc.h"
#include <mocha/commands.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <unistd.h>

typedef enum {
    MOCHA_LOAD_TARGET_DEVICE_NONE = 0,
    MOCHA_LOAD_TARGET_DEVICE_SD   = 1,
    MOCHA_LOAD_TARGET_DEVICE_MLC  = 2,
} LoadTargetDevice;

static int
MCP_LoadCustomFile(LoadTargetDevice target, char *path, uint32_t filesize, uint32_t fileoffset, void *buffer_out,
                   uint32_t buffer_len,
                   uint32_t pos, bool isRPX);

int (*const real_MCP_LoadFile)(ipcmessage *msg) = (int (*)(ipcmessage *))(0x0501CAA8 + 1); //+1 for thumb
int (*const MCP_DoLoadFile)(const char *path,
                            const char *path2,
                            void *outputBuffer,
                            uint32_t outLength,
                            uint32_t pos,
                            int *bytesRead,
                            uint32_t unk)       = (int (*)(const char *,
                                                     const char *,
                                                     void *,
                                                     uint32_t,
                                                     uint32_t,
                                                     int *,
                                                     uint32_t))(0x05017248 + 1);

typedef enum {
    REPLACEMENT_TYPE_INVALID                              = 0,
    REPLACEMENT_TYPE_HOMEBREW_RPX                         = 1,
    REPLACEMENT_TYPE_HOMEBREW_RPL                         = 2,
    REPLACEMENT_TYPE_BY_PATH                              = 3,
    REPLACEMENT_TYPE_UNTIL_MEM_HOOK_COMPLETED_BUT_BY_PATH = 4,
} ReplacementType;

typedef enum {
    REPLACEMENT_LIFETIME_INVALID                = 0,
    REPLACEMENT_LIFETIME_UNLIMITED              = 1,
    REPLACEMENT_LIFETIME_ONE_RPX_LAUNCH         = 2,
    REPLACEMENT_LIFETIME_DURING_RPX_REPLACEMENT = 3,
} ReplacementLifetime;

typedef enum {
    PATH_RELATIVE_INVALID        = 0,
    PATH_RELATIVE_TO_ENVIRONMENT = 1,
    PATH_RELATIVE_TO_SD_ROOT     = 2,
    PATH_RELATIVE_TO_MLC_ROOT    = 3,
} PathRelativeTo;


typedef struct RPXFileReplacements {
    ReplacementType type;
    ReplacementLifetime lifetime;
    char replaceName[0x40];
    char replacementPath[256];
    PathRelativeTo relativeTo;
    uint32_t fileSize;
    uint32_t fileOffset;
    uint32_t ageInApplicationStarts;
} RPXFileReplacements;

static bool gMemHookCompleted = false;
static bool sReplacedLastRPX  = false;


// Dynamic replacements have priority over default replacements
static RPXFileReplacements *gDynamicReplacements[5] = {};

static const RPXFileReplacements gDefaultReplacements[] = {
        // Redirect men.rpx [ENVIRONMENT]/root.rpx until IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED has been called. (e.g. to init PPC homebrew after iosu-reload)
        {REPLACEMENT_TYPE_UNTIL_MEM_HOOK_COMPLETED_BUT_BY_PATH, REPLACEMENT_LIFETIME_UNLIMITED, "men.rpx", "root.rpx", PATH_RELATIVE_TO_ENVIRONMENT, 0, 0, 0},
        // Redirect men.rpx to [ENVIRONMENT]/men.rpx
        {REPLACEMENT_TYPE_BY_PATH, REPLACEMENT_LIFETIME_UNLIMITED, "men.rpx", "men.rpx", PATH_RELATIVE_TO_ENVIRONMENT, 0, 0, 0},
        // Try to load the real H&S safe.rpx from backup
        {REPLACEMENT_TYPE_BY_PATH, REPLACEMENT_LIFETIME_UNLIMITED, "safe.rpx", "sys/title/00050010/1004e000/content/safe.rpx.bak", PATH_RELATIVE_TO_MLC_ROOT, 0, 0, 0},
        {REPLACEMENT_TYPE_BY_PATH, REPLACEMENT_LIFETIME_UNLIMITED, "safe.rpx", "sys/title/00050010/1004e100/content/safe.rpx.bak", PATH_RELATIVE_TO_MLC_ROOT, 0, 0, 0},
        {REPLACEMENT_TYPE_BY_PATH, REPLACEMENT_LIFETIME_UNLIMITED, "safe.rpx", "sys/title/00050010/1004e200/content/safe.rpx.bak", PATH_RELATIVE_TO_MLC_ROOT, 0, 0, 0},
};


// should be at least the size of gDefaultReplacements and gDynamicReplacements
#define TEMP_ARRAY_SIZE 10

bool addDynamicReplacement(RPXFileReplacements *pReplacements) {
    for (uint32_t i = 0; i < sizeof(gDynamicReplacements) / sizeof(gDynamicReplacements[0]); i++) {
        if (gDynamicReplacements[i] == NULL) {
            gDynamicReplacements[i] = pReplacements;
            return true;
        }
    }
    DEBUG_FUNCTION_LINE("Failed to find empty slot!\n");
    return false;
}

void RemoveByLifetime(ReplacementLifetime lifetime) {
    for (uint32_t i = 0; i < sizeof(gDynamicReplacements) / sizeof(gDynamicReplacements[0]); i++) {
        if (gDynamicReplacements[i] != NULL && gDynamicReplacements[i]->lifetime == lifetime) {
            // We DON'T want to remove new entries which had the chance to be used
            // There are intended to be used for the NEXT application start.
            if (lifetime == REPLACEMENT_LIFETIME_DURING_RPX_REPLACEMENT &&
                gDynamicReplacements[i]->ageInApplicationStarts == 0) {
                continue;
            }
            /*DEBUG_FUNCTION_LINE("Remove item %p: lifetime: %d type: %d replace: %s replaceWith: %s\n",
                                gDynamicReplacements[i], gDynamicReplacements[i]->lifetime,
                                gDynamicReplacements[i]->type, gDynamicReplacements[i]->replaceName,
                                gDynamicReplacements[i]->replacementPath);*/
            svcFree(0xCAFF, gDynamicReplacements[i]);
            gDynamicReplacements[i] = NULL;
        }
    }
}

static inline int EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr    = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// Set filesize to 0 if unknown.
static int
MCP_LoadCustomFile(LoadTargetDevice target, char *path, uint32_t filesize, uint32_t fileoffset, void *buffer_out,
                   uint32_t buffer_len,
                   uint32_t pos, bool isRPX) {
    if (path == NULL || (filesize > 0 && (pos > filesize))) {
        return 0;
    }

    char filepath[256];
    memset(filepath, 0, sizeof(filepath));
    strncpy(filepath, path, sizeof(filepath) - 1);

    char mountpath[] = "/vol/storage_iosu_homebrew";

    if (target == MOCHA_LOAD_TARGET_DEVICE_SD) {
        int fsa_h = svcOpen("/dev/fsa", 0);
        FSA_Mount(fsa_h, "/dev/sdcard01", mountpath, 2, NULL, 0);
        svcClose(fsa_h);

        strncpy(filepath, mountpath, sizeof(filepath) - 1);
        strncat(filepath, "/", (sizeof(filepath) - 1) - strlen(filepath));
        strncat(filepath, path, (sizeof(filepath) - 1) - strlen(filepath));
    }

    DEBUG_FUNCTION_LINE("Load custom path \"%s\"\n", filepath);

    int bytesRead = 0;
    int result    = MCP_DoLoadFile(filepath, NULL, buffer_out, buffer_len, pos + fileoffset, &bytesRead, 0);
    // DEBUG_FUNCTION_LINE("MCP_DoLoadFile returned %d, bytesRead = %d pos %u\n", result, bytesRead, pos + fileoffset);

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

    // Unmount the sd card once the whole file has been read or there was an error
    if ((result >= 0 && result < 0x400000) || result < 0) {
        if (target == MOCHA_LOAD_TARGET_DEVICE_SD) {
            int fsa_h = svcOpen("/dev/fsa", 0);
            FSA_Unmount(fsa_h, mountpath, 0x80000002);
            svcClose(fsa_h);
        }
        if (isRPX) {
            // if we have read less than 0x400000 bytes we have read the whole file.
            // We can now remove the replacements with "one launch" lifetime
            RemoveByLifetime(REPLACEMENT_LIFETIME_ONE_RPX_LAUNCH);
        }
    }

    return result;
}

int DoSDRedirectionByPath(ipcmessage *msg, MCPLoadFileRequest *request) {
    if (strlen(request->name) > 1 && request->name[0] == '~' && request->name[1] == '|') {
        // OSDynload_Acquire is cutting of the name right after the last '/'. This means "~/wiiu/libs/test.rpl" would simply become "test.rpl".
        // To still have directories, Mocha expects '|' instead of '/'. (Modules like the AromaBaseModule might handle this transparent for the user.)
        // Example: "~|wiiu|libs|test.rpl" would load "sd://wiiu/libs/test.rpl".
        char *curPtr = &request->name[1];
        while (*curPtr != '\0') {
            if (*curPtr == '|') {
                *curPtr = '/';
            }
            curPtr++;
        }
        // DEBUG_FUNCTION_LINE("Trying to load %s from sd\n", &request->name[2]);
        int result = MCP_LoadCustomFile(MOCHA_LOAD_TARGET_DEVICE_SD, &request->name[2], 0, 0, msg->ioctl.buffer_io,
                                        msg->ioctl.length_io, request->pos, EndsWith(request->name, ".rpx"));

        if (result >= 0) {
            return result;
        } else {
            DEBUG_FUNCTION_LINE("Failed: result was %d\n", result);
        }
        return real_MCP_LoadFile(msg);
    }
    return -1;
}

bool isCurrentHomebrewWrapperReplacement(MCPLoadFileRequest *request) {
    if ((strncmp("ply.rpx", request->name, strlen("ply.rpx")) == 0) ||
        (strncmp("safe.rpx", request->name, strlen("safe.rpx")) == 0)) {
        return true;
    }
    return false;
}

const RPXFileReplacements *GetCurrentRPXReplacementEx(MCPLoadFileRequest *request, const RPXFileReplacements **list, uint32_t list_size, int *offset) {
    if (!offset || *offset >= list_size) {
        return NULL;
    }
    for (uint32_t i = *offset; i < list_size; i++) {
        const RPXFileReplacements *cur = list[i];
        if (cur == NULL || cur->lifetime == REPLACEMENT_LIFETIME_INVALID) {
            continue;
        }
        bool valid = false;
        switch (cur->type) {
            case REPLACEMENT_TYPE_HOMEBREW_RPX: {
                // DEBUG_FUNCTION_LINE("Check REPLACEMENT_TYPE_HOMEBREW_RPX for %s... ", cur->replacementPath);
                if (isCurrentHomebrewWrapperReplacement(request)) {
                    // printf("Yay!\n");
                    valid = true;
                } else {
                    // printf("No :(!\n");
                }
                break;
            }
            case REPLACEMENT_TYPE_HOMEBREW_RPL:
                // DEBUG_FUNCTION_LINE("Check REPLACEMENT_TYPE_HOMEBREW_RPL for %s (%s) == %s (sReplacedLastRPX %d)... ", cur->replaceName,
                //                    cur->replacementPath, request->name, sReplacedLastRPX);
                if (sReplacedLastRPX && strncmp(cur->replaceName, request->name, sizeof(request->name) - 1) == 0) {
                    // printf("Yay!\n");
                    valid = true;
                } else {
                    // printf("No :(!\n");
                }
                break;
            case REPLACEMENT_TYPE_BY_PATH:
                // DEBUG_FUNCTION_LINE("Check REPLACEMENT_TYPE_BY_PATH for %s (%s) == %s... ", cur->replaceName,
                //                    cur->replacementPath, request->name);
                if (strncmp(cur->replaceName, request->name, sizeof(request->name) - 1) == 0) {
                    // printf("Yay!\n");
                    valid = true;
                } else {
                    // printf("No :(!\n");
                }
                break;
            case REPLACEMENT_TYPE_UNTIL_MEM_HOOK_COMPLETED_BUT_BY_PATH:
                // DEBUG_FUNCTION_LINE("Check REPLACEMENT_TYPE_UNTIL_MEM_HOOK_COMPLETED_BUT_BY_PATH for %s (%s) == %s... ", cur->replaceName,
                //                    cur->replacementPath, request->name);
                if (strncmp(cur->replaceName, request->name, sizeof(request->name) - 1) == 0 && !gMemHookCompleted) {
                    // printf("Yay!\n");
                    valid = true;
                } else {
                    // printf("No :(!\n");
                }
                break;
            case REPLACEMENT_TYPE_INVALID:
                DEBUG_FUNCTION_LINE("REPLACEMENT_TYPE_INVALID\n");
                break;
        }
        if (valid) {
            *offset = i + 1;
            return cur;
        }
    }
    return NULL;
}

static uint32_t getReplacementDataInSingleArray(const RPXFileReplacements **tmpArray, uint32_t tmpArraySize) {
    uint32_t offsetInResult = 0;

    for (uint32_t i = 0; i < sizeof(gDynamicReplacements) / sizeof(gDynamicReplacements[0]); i++) {
        if (offsetInResult >= tmpArraySize) {
            DEBUG_FUNCTION_LINE("ELEMENTS DO NOT FIT INTO ARRAY");
            return offsetInResult;
        }
        if (gDynamicReplacements[i] != NULL) {
            tmpArray[offsetInResult] = gDynamicReplacements[i];
            offsetInResult++;
        }
    }

    for (uint32_t i = 0; i < sizeof(gDefaultReplacements) / sizeof(gDefaultReplacements[0]); i++) {
        if (offsetInResult >= tmpArraySize) {
            DEBUG_FUNCTION_LINE("ELEMENTS DO NOT FIT INTO ARRAY");
            return offsetInResult;
        }
        if (gDefaultReplacements[i].lifetime != REPLACEMENT_LIFETIME_INVALID) {
            tmpArray[offsetInResult] = &gDefaultReplacements[i];
            offsetInResult++;
        }
    }
    return offsetInResult;
}

const RPXFileReplacements *GetCurrentRPXReplacement(MCPLoadFileRequest *request, int *offset) {
    const RPXFileReplacements *tmpArray[TEMP_ARRAY_SIZE] = {};
    uint32_t elementsInArray                             = getReplacementDataInSingleArray(tmpArray, TEMP_ARRAY_SIZE);
    if (elementsInArray == 0) {
        return NULL;
    }

    return GetCurrentRPXReplacementEx(request, tmpArray, elementsInArray, offset);
}

int DoReplacementByStruct(ipcmessage *msg, MCPLoadFileRequest *request, const RPXFileReplacements *curReplacement) {
    char _rpxpath[256] = {};

    LoadTargetDevice target = MOCHA_LOAD_TARGET_DEVICE_NONE;
    if (curReplacement->relativeTo == PATH_RELATIVE_TO_ENVIRONMENT) {
        char *environmentPath = &((char *) 0x0511FF00)[19];
        snprintf(_rpxpath, sizeof(_rpxpath) - 1, "%s/%s", environmentPath, curReplacement->replacementPath); // Copy in environment path
        target = MOCHA_LOAD_TARGET_DEVICE_SD;
    } else if (curReplacement->relativeTo == PATH_RELATIVE_TO_SD_ROOT) {
        strncpy(_rpxpath, curReplacement->replacementPath, sizeof(_rpxpath) - 1);
        target = MOCHA_LOAD_TARGET_DEVICE_SD;
    } else if (curReplacement->relativeTo == PATH_RELATIVE_TO_MLC_ROOT) {
        strncpy(_rpxpath, curReplacement->replacementPath, sizeof(_rpxpath) - 1);
        snprintf(_rpxpath, sizeof(_rpxpath) - 1, "/vol/storage_mlc01/%s", curReplacement->replacementPath); // Copy in environment path
        target = MOCHA_LOAD_TARGET_DEVICE_MLC;
    } else {
        DEBUG_FUNCTION_LINE("Unknown relativeTo: %d\n", curReplacement->relativeTo);
        return -1;
    }

    DEBUG_FUNCTION_LINE("Load custom file %s\n", _rpxpath);
    return MCP_LoadCustomFile(target,
                              _rpxpath,
                              curReplacement->fileSize,
                              curReplacement->fileOffset,
                              msg->ioctl.buffer_io,
                              msg->ioctl.length_io,
                              request->pos, EndsWith(request->name, ".rpx"));
}

int MCPLoadFileReplacement(ipcmessage *msg, MCPLoadFileRequest *request) {
    int res;

    if ((res = DoSDRedirectionByPath(msg, request)) >= 0) {
        // DEBUG_FUNCTION_LINE("We replaced by path!\n");
        return res;
    }

    // Try any fitting rpx replacement
    int offset                                = 0;
    const RPXFileReplacements *curReplacement = NULL;
    do {
        // the offset is used as input AND output.
        // The start index for looking for a replacement will be read from "offset"
        // if the function returns != NULL, offset will be set to the next possible index/offset
        curReplacement = GetCurrentRPXReplacement(request, &offset);
        if (curReplacement == NULL) {
            // DEBUG_FUNCTION_LINE("Couldn't find replacement for %s!\n", request->name);
            return -1;
        }

        if ((res = DoReplacementByStruct(msg, request, curReplacement)) >= 0) {
            return res;
        }
    } while (curReplacement);

    return -1;
}

void IncreaseApplicationStartCounter() {
    for (uint32_t i = 0; i < sizeof(gDynamicReplacements) / sizeof(gDynamicReplacements[0]); i++) {
        if (gDynamicReplacements[i] != NULL) {
            gDynamicReplacements[i]->ageInApplicationStarts++;
        }
    }
}

bool hasHomebrewReplacementsEx(const RPXFileReplacements **list, uint32_t list_size) {
    for (uint32_t i = 0; i < list_size; i++) {
        const RPXFileReplacements *cur = list[i];
        if (cur != NULL && cur->type != REPLACEMENT_TYPE_INVALID) {
            if (cur->type == REPLACEMENT_TYPE_HOMEBREW_RPX) {
                return true;
            }
        }
    }
    return false;
}

bool hasHomebrewReplacements() {
    const RPXFileReplacements *tmpArray[TEMP_ARRAY_SIZE] = {};
    uint32_t elementsInArray                             = getReplacementDataInSingleArray(tmpArray, TEMP_ARRAY_SIZE);
    if (elementsInArray == 0) {
        return false;
    }

    if (!hasHomebrewReplacementsEx(tmpArray, elementsInArray)) {
        // DEBUG_FUNCTION_LINE("Has no homebrew replacements\n");
        return false;
    }
    return true;
}

int _MCP_LoadFile_patch(ipcmessage *msg) {
    MCPLoadFileRequest *request = (MCPLoadFileRequest *) msg->ioctl.buffer_in;

    // we only care about Foreground app/COS-MASTER for now.
    if (request->cafe_pid != 7) {
        // DEBUG_FUNCTION_LINE("Not for pid 7, lets ignore\n");
        return real_MCP_LoadFile(msg);
    }

    bool requestIsRPX = EndsWith(request->name, ".rpx");

    // Check if a fresh RPX is loaded
    if (request->pos == 0 && requestIsRPX) {
        // DEBUG_FUNCTION_LINE("fresh RPX load\n");
        // Remove any old replacements that should only survive one RPX replacement
        RemoveByLifetime(REPLACEMENT_LIFETIME_DURING_RPX_REPLACEMENT);
        // Increase of the all replacements when we load a new RPX!
        IncreaseApplicationStartCounter();
        sReplacedLastRPX = false;
    }

    int res;
    if ((res = MCPLoadFileReplacement(msg, request)) >= 0) {
        if (requestIsRPX) {
            sReplacedLastRPX = true;
        }
        return res;
    }

    res = real_MCP_LoadFile(msg);
    return res;
}

int _MCP_ReadCOSXml_patch(uint32_t u1, uint32_t u2, MCPPPrepareTitleInfo *xmlData) {
    int (*const real_MCP_ReadCOSXml_patch)(uint32_t u1, uint32_t u2, MCPPPrepareTitleInfo * xmlData) = (void *) 0x050024ec + 1; //+1 for thumb

    int res = real_MCP_ReadCOSXml_patch(u1, u2, xmlData);

    /*
    For some reason ONE PIECE Unlimited World Red softlocks when it has full FSA permissions.
    */
    if (xmlData->titleId != 0x0005000010175C00 && // ONE PIECE Unlimited World Red USA
        xmlData->titleId != 0x0005000010175D00 && // ONE PIECE Unlimited World Red EUR
        xmlData->titleId != 0x0005000010148000) { // ONE PIECE Unlimited World Red JPN

        // Give us sd access!
        xmlData->permissions[4].mask = 0xFFFFFFFFFFFFFFFF;
    }
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
    if (hasHomebrewReplacements()) {
        if (xmlData->titleId == 0x000500101004E000 || // H&S
            xmlData->titleId == 0x000500101004E100 ||
            xmlData->titleId == 0x000500101004E200 ||
            xmlData->titleId == 0x000500101004C000 || // Daily log
            xmlData->titleId == 0x000500101004C100 ||
            xmlData->titleId == 0x000500101004C200) {
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
    if (!gMemHookCompleted) {
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
    if (msg->ioctl.buffer_in && msg->ioctl.length_in >= 4) {
        int command = msg->ioctl.buffer_in[0];

        switch (command) {
            case IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED: {
                gMemHookCompleted = true;
                break;
            }
            case IPC_CUSTOM_LOAD_CUSTOM_RPX: {
                if (msg->ioctl.length_in >= sizeof(MochaRPXLoadInfo) + 4) {
                    int target = msg->ioctl.buffer_in[0x04 / 0x04];
                    if (target == LOAD_RPX_TARGET_EXTRA_REVERT_PREPARE) {
                        return 0;
                    }
                    if (target != LOAD_RPX_TARGET_SD_CARD) {
                        return 29;
                    }
                    uint32_t filesize   = msg->ioctl.buffer_in[0x08 / 0x04];
                    uint32_t fileoffset = msg->ioctl.buffer_in[0x0C / 0x04];
                    char *str_ptr       = (char *) &msg->ioctl.buffer_in[0x10 / 0x04];

                    RPXFileReplacements *newReplacement = svcAlloc(0xCAFF, sizeof(RPXFileReplacements));
                    if (newReplacement == NULL) {
                        DEBUG_FUNCTION_LINE("Failed to allocate memory on heap\n");
                        return 22;
                    }

                    memset(newReplacement, 0, sizeof(*newReplacement));

                    strncpy(newReplacement->replacementPath, str_ptr, sizeof(newReplacement->replacementPath) - 1);
                    newReplacement->fileOffset = fileoffset;
                    newReplacement->fileSize   = filesize;
                    newReplacement->lifetime   = REPLACEMENT_LIFETIME_ONE_RPX_LAUNCH;
                    newReplacement->relativeTo = PATH_RELATIVE_TO_SD_ROOT;
                    newReplacement->type       = REPLACEMENT_TYPE_HOMEBREW_RPX;

                    if (!addDynamicReplacement(newReplacement)) {
                        DEBUG_FUNCTION_LINE("addDynamicReplacement failed, abort redirecting %s\n", newReplacement->replacementPath);
                        svcFree(0xCAFF, newReplacement);
                        return 22;
                    }

                    DEBUG_FUNCTION_LINE("Will load %s for next title from target: %d (offset %u, filesize %u)\n",
                                        newReplacement->replacementPath, target, fileoffset, filesize);
                    return 0;
                } else {
                    return 29;
                }
                break;
            }
            case IPC_CUSTOM_START_MCP_THREAD: {
                _startMainThread();
                return 0;
                break;
            }
            case IPC_CUSTOM_COPY_ENVIRONMENT_PATH: {
                if (msg->ioctl.buffer_io && msg->ioctl.length_io >= 0x100) {
                    strncpy((char *) msg->ioctl.buffer_io, (void *) 0x0511FF00, 0xFF);
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

                // set the flag to not run this twice.
                svcCustomKernelCommand(KERNEL_WRITE32, 0x050290dc, 0x42424242);

                // Patch MCP debugmode check for usb syslog
                svcCustomKernelCommand(KERNEL_WRITE32, 0x050290d8, 0x20004770);
                // Patch TEST to allow usb syslog
                svcCustomKernelCommand(KERNEL_WRITE32, 0xe4007828, 0xe3a00000);

                usleep(1000 * 10);

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

                bool showCompleteLog = msg->ioctl.buffer_in && msg->ioctl.length_in >= 0x08 && msg->ioctl.buffer_in[1] == 1;
                if (!showCompleteLog) {
                    // Kill existing syslogs to avoid long catch up
                    uint32_t *bufferPtr = (uint32_t *) (*(uint32_t *) 0x05095ecc);
                    bufferPtr[0]        = 0;
                    bufferPtr[1]        = 0;
                }

                break;
            }
            case IPC_CUSTOM_GET_MOCHA_API_VERSION: {
                if (msg->ioctl.buffer_io && msg->ioctl.length_io >= 8) {
                    msg->ioctl.buffer_io[0] = 0xCAFEBABE;
                    msg->ioctl.buffer_io[1] = MOCHA_API_VERSION;
                    return 0;
                } else {
                    return 29;
                }
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
