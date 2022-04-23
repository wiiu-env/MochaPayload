#ifndef FSA_H
#define FSA_H

#include "types.h"
#include <assert.h>

typedef struct __attribute__((__packed__)) FSStat {
   u32 flags;
   u32 mode;
   u32 owner;
   u32 group;
   u32 size;
   u32 allocSize;
   u64 quotaSize;
   u32 entryId;
   u64 created;
   u64 modified;
   u8 attributes[48];
} FSStat;
static_assert(sizeof(FSStat) == 0x64);

typedef struct FSDirectory {
   FSStat info;
   char name[256];
} FSDirectory;
static_assert(sizeof(FSDirectory) == 0x164);

typedef struct {
    u8 unknown[0x1E];
} FSFileSystemInfo;

typedef struct {
    u8 unknown[0x28];
} FSDeviceInfo;

typedef struct {
    u64 blocks_count;
    u64 some_count;
    u32 block_size;
} FSBlockInfo;

int FSA_Mount(int fd, char *device_path, char *volume_path, u32 flags, char *arg_string, int arg_string_len);
int FSA_Unmount(int fd, char *path, u32 flags);
int FSA_FlushVolume(int fd, char *volume_path);
int FSA_RollbackVolume(int fd, char *volume_path);

int FSA_GetInfo(int fd, char *device_path, int type, u32 *out_data);
int FSA_GetStat(int fd, char *path, FSStat *out_data);

int FSA_MakeDir(int fd, char *path, u32 flags);
int FSA_OpenDir(int fd, char *path, int *outHandle);
int FSA_ReadDir(int fd, int handle, FSDirectory *out_data);
int FSA_RewindDir(int fd, int handle);
int FSA_CloseDir(int fd, int handle);
int FSA_ChangeDir(int fd, char *path);
int FSA_GetCwd(int fd, char *out_data, int output_size);

int FSA_MakeQuota(int fd, char *path, u32 flags, u64 size);
int FSA_FlushQuota(int fd, char *quota_path);
int FSA_RollbackQuota(int fd, char *quota_path);
int FSA_RollbackQuotaForce(int fd, char *quota_path);

int FSA_OpenFile(int fd, char *path, char *mode, int *outHandle);
int FSA_OpenFileEx(int fd, char *path, char *mode, int *outHandle, u32 flags, int create_mode, u32 create_alloc_size);
int FSA_ReadFile(int fd, void *data, u32 size, u32 cnt, int fileHandle, u32 flags);
int FSA_WriteFile(int fd, void *data, u32 size, u32 cnt, int fileHandle, u32 flags);
int FSA_ReadFileWithPos(int fd, void *data, u32 size, u32 cnt, u32 position, int fileHandle, u32 flags);
int FSA_WriteFileWithPos(int fd, void *data, u32 size, u32 cnt, u32 position, int fileHandle, u32 flags);
int FSA_AppendFile(int fd, u32 size, u32 cnt, int fileHandle);
int FSA_AppendFileEx(int fd, u32 size, u32 cnt, int fileHandle, u32 flags);
int FSA_CloseFile(int fd, int fileHandle);
int FSA_GetStatFile(int fd, int handle, FSStat *out_data);
int FSA_FlushFile(int fd, int handle);
int FSA_TruncateFile(int fd, int handle);
int FSA_GetPosFile(int fd, int fileHandle, u32 *out_position);
int FSA_SetPosFile(int fd, int fileHandle, u32 position);
int FSA_IsEof(int fd, int fileHandle);

int FSA_Remove(int fd, char *path);
int FSA_Rename(int fd, char *old_path, char *new_path);
int FSA_ChangeMode(int fd, char *path, int mode);
int FSA_ChangeModeEx(int fd, char *path, int mode, int mask);
int FSA_ChangeOwner(int fd, char *path, u32 owner, u32 group);

int FSA_RawOpen(int fd, char *device_path, int *outHandle);
int FSA_RawRead(int fd, void *data, u32 size_bytes, u32 cnt, u64 sector_offset, int device_handle);
int FSA_RawWrite(int fd, void *data, u32 size_bytes, u32 cnt, u64 sector_offset, int device_handle);
int FSA_RawClose(int fd, int device_handle);

#endif
