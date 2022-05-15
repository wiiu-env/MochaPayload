#include "fsa.h"
#include "imports.h"
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *allocIobuf() {
    void *ptr = svcAlloc(0xCAFF, 0x828);
    memset(ptr, 0x00, 0x828);
    return ptr;
}

static void freeIobuf(void *ptr) {
    svcFree(0xCAFF, ptr);
}

static int _ioctl_fd_path_internal(int fd, int ioctl_num, int num_args, char *path, u32 arg1, u32 arg2, u32 arg3, u32 *out_data, u32 out_data_size) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    switch (num_args) {
        case 4:
            inbuf[0x28C / 4] = arg3;
        case 3:
            inbuf[0x288 / 4] = arg2;
        case 2:
            inbuf[0x284 / 4] = arg1;
        case 1:
            strncpy((char *) &inbuf[0x01], path, 0x27F);
    }
    int ret = svcIoctl(fd, ioctl_num, inbuf, 0x520, outbuf, 0x293);
    if (out_data && out_data_size) {
        memcpy(out_data, &outbuf[1], out_data_size);
    }

    freeIobuf(iobuf);
    return ret;
}

static int _ioctl_fd_handle_internal(int fd, int ioctl_num, int num_args, int handle, u32 arg1, u32 arg2, u32 arg3, u32 *out_data, u32 out_data_size) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    switch (num_args) {
        case 4:
            inbuf[0x28C / 4] = arg3;
        case 3:
            inbuf[0x288 / 4] = arg2;
        case 2:
            inbuf[0x284 / 4] = arg1;
        case 1:
            inbuf[0x01] = handle;
    }
    int ret = svcIoctl(fd, ioctl_num, inbuf, 0x520, outbuf, 0x293);
    if (out_data && out_data_size) {
        memcpy(out_data, &outbuf[1], out_data_size);
    }

    freeIobuf(iobuf);
    return ret;
}

// Now make all required wrappers
// clang-format off
#define dispatch_ioctl_arg4_out(fd, ioctl_num, handle_or_path, arg1, arg2, arg3, out_data, out_data_size) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 4, handle_or_path, arg1, arg2, arg3, out_data, out_data_size)
#define dispatch_ioctl_arg3_out(fd, ioctl_num, handle_or_path, arg1, arg2, out_data, out_data_size) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 3, handle_or_path, arg1, arg2, 0, out_data, out_data_size)
#define dispatch_ioctl_arg2_out(fd, ioctl_num, handle_or_path, arg1, out_data, out_data_size) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 2, handle_or_path, arg1, 0, 0, out_data, out_data_size)
#define dispatch_ioctl_arg1_out(fd, ioctl_num, handle_or_path, out_data, out_data_size) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 1, handle_or_path, 0, 0, 0, out_data, out_data_size)

#define dispatch_ioctl_arg4(fd, ioctl_num, handle_or_path, arg1, arg2, arg3) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 4, handle_or_path, arg1, arg2, arg3, NULL, 0)
#define dispatch_ioctl_arg3(fd, ioctl_num, handle_or_path, arg1, arg2) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 3, handle_or_path, arg1, arg2, 0, NULL, 0)
#define dispatch_ioctl_arg2(fd, ioctl_num, handle_or_path, arg1) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 2, handle_or_path, arg1, 0, 0, NULL, 0)
#define dispatch_ioctl_arg1(fd, ioctl_num, handle_or_path) _Generic((handle_or_path), char*: _ioctl_fd_path_internal, int: _ioctl_fd_handle_internal)(fd, ioctl_num, 1, handle_or_path, 0, 0, 0, NULL, 0)

#define GET_MACRO(_1,_2,_3,_4,_5,_6, MACRO_NAME, ...) MACRO_NAME
#define dispatch_ioctl_out(fd, ioctl_num, ...) GET_MACRO(__VA_ARGS__, dispatch_ioctl_arg4_out, dispatch_ioctl_arg3_out, dispatch_ioctl_arg2_out, dispatch_ioctl_arg1_out, NULL, NULL)(fd, ioctl_num, __VA_ARGS__)
#define dispatch_ioctl(fd, ioctl_num, ...) GET_MACRO(__VA_ARGS__,  NULL, NULL, dispatch_ioctl_arg4, dispatch_ioctl_arg3, dispatch_ioctl_arg2, dispatch_ioctl_arg1)(fd, ioctl_num, __VA_ARGS__)
// clang-format on

int FSA_Mount(int fd, char *device_path, char *volume_path, u32 flags, char *arg_string, int arg_string_len) {
    u8 *iobuf      = allocIobuf();
    u8 *inbuf8     = iobuf;
    u8 *outbuf8    = &iobuf[0x520];
    iovec_s *iovec = (iovec_s *) &iobuf[0x7C0];
    u32 *inbuf     = (u32 *) inbuf8;
    u32 *outbuf    = (u32 *) outbuf8;

    strncpy((char *) &inbuf8[0x04], device_path, 0x27F);
    strncpy((char *) &inbuf8[0x284], volume_path, 0x27F);
    inbuf[0x504 / 4] = (u32) flags;
    inbuf[0x508 / 4] = (u32) arg_string_len;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;
    iovec[1].ptr = arg_string;
    iovec[1].len = arg_string_len;
    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret = svcIoctlv(fd, 0x01, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}

int FSA_Unmount(int fd, char *path, u32 flags) {
    return dispatch_ioctl(fd, 0x02, path, flags);
}

int FSA_FlushVolume(int fd, char *volume_path) {
    return dispatch_ioctl(fd, 0x1B, volume_path);
}

int FSA_RollbackVolume(int fd, char *volume_path) {
    return dispatch_ioctl(fd, 0x1C, volume_path);
}

// type:
// 0 - FSA_GetFreeSpaceSize
// 1 - FSA_GetDirSize
// 2 - FSA_GetEntryNum
// 3 - FSA_GetFileSystemInfo
// 4 - FSA_GetDeviceInfo
// 5 - FSA_GetStat
// 6 - FSA_GetBadBlockInfo
// 7 - FSA_GetJournalFreeSpace
// 8 - FSA_GetFragmentBlockInfo
int FSA_GetInfo(int fd, char *device_path, int type, u32 *out_data) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];
    strncpy((char *) &inbuf[0x01], device_path, 0x27F);
    inbuf[0x284 / 4] = type;
    int ret          = svcIoctl(fd, 0x18, inbuf, 0x520, outbuf, 0x293);
    int size         = 0;
    switch (type) {
        case 0:
        case 1:
        case 7:
            size = sizeof(u64);
            break;
        case 2:
            size = sizeof(u32);
            break;
        case 3:
            size = sizeof(FSFileSystemInfo);
            break;
        case 4:
            size = sizeof(FSDeviceInfo);
            break;
        case 5:
            size = sizeof(FSStat);
            break;
        case 6:
        case 8:
            size = sizeof(FSBlockInfo);
            break;
    }

    memcpy(out_data, &outbuf[1], size);
    freeIobuf(iobuf);
    return ret;
}

int FSA_GetStat(int fd, char *path, FSStat *out_data) {
    return FSA_GetInfo(fd, path, 5, (u32 *) out_data);
}

int FSA_MakeDir(int fd, char *path, u32 flags) {
    return dispatch_ioctl(fd, 0x07, path, flags);
}

int FSA_OpenDir(int fd, char *path, int *outHandle) {
    return dispatch_ioctl_out(fd, 0x0A, path, (u32 *) outHandle, sizeof(int));
}

int FSA_ReadDir(int fd, int handle, FSDirectory *out_data) {
    return dispatch_ioctl_out(fd, 0x0B, handle, (u32 *) out_data, sizeof(FSDirectory));
}

int FSA_RewindDir(int fd, int handle) {
    return dispatch_ioctl(fd, 0x0C, handle);
}

int FSA_CloseDir(int fd, int handle) {
    return dispatch_ioctl(fd, 0x0D, handle);
}

int FSA_ChangeDir(int fd, char *path) {
    return dispatch_ioctl(fd, 0x05, path);
}

int FSA_GetCwd(int fd, char *out_data, int output_size) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    int ret = svcIoctl(fd, 0x06, inbuf, 0x520, outbuf, 0x293);

    if (output_size > 0x27F) output_size = 0x27F;
    if (out_data) strncpy(out_data, (char *) &outbuf[1], output_size);

    freeIobuf(iobuf);
    return ret;
}


int FSA_MakeQuota(int fd, char *path, u32 flags, u64 size) {
    return dispatch_ioctl(fd, 0x07, path, flags, (size >> 32), (size & 0xFFFFFFFF));
}

int FSA_FlushQuota(int fd, char *quota_path) {
    return dispatch_ioctl(fd, 0x1E, quota_path);
}

int FSA_RollbackQuota(int fd, char *quota_path) {
    return dispatch_ioctl(fd, 0x1F, quota_path, 0);
}

int FSA_RollbackQuotaForce(int fd, char *quota_path) {
    return dispatch_ioctl(fd, 0x1F, quota_path, 0x80000000);
}

int FSA_RegisterFlushQuota(int fd, char *quota_path) {
    return dispatch_ioctl(fd, 0x22, quota_path);
}

int FSA_FlushMultiQuota(int fd, char *quota_path) {
    return dispatch_ioctl(fd, 0x23, quota_path);
}

int FSA_OpenFile(int fd, char *path, char *mode, int *outHandle) {
    return FSA_OpenFileEx(fd, path, mode, outHandle, 0, 0, 0);
}

int FSA_OpenFileEx(int fd, char *path, char *mode, int *outHandle, u32 flags, int create_mode, u32 create_alloc_size) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    strncpy((char *) &inbuf[0x01], path, 0x27F);
    strncpy((char *) &inbuf[0xA1], mode, 0x10);
    inbuf[0x294 / 4] = flags;
    inbuf[0x298 / 4] = create_mode;
    inbuf[0x29C / 4] = create_alloc_size;

    int ret = svcIoctl(fd, 0x0E, inbuf, 0x520, outbuf, 0x293);

    if (outHandle) *outHandle = outbuf[1];

    freeIobuf(iobuf);
    return ret;
}

int _FSA_ReadWriteFileWithPos(int fd, void *data, u32 size, u32 cnt, u32 pos, int fileHandle, u32 flags, bool read) {
    u8 *iobuf      = allocIobuf();
    u32 *outbuf    = (u32 *) &iobuf[0x520];
    iovec_s *iovec = (iovec_s *) &iobuf[0x7C0];
    u32 *inbuf     = (u32 *) iobuf;

    inbuf[0x08 / 4] = size;
    inbuf[0x0C / 4] = cnt;
    inbuf[0x10 / 4] = pos;
    inbuf[0x14 / 4] = fileHandle;
    inbuf[0x18 / 4] = flags;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;

    iovec[1].ptr = data;
    iovec[1].len = size * cnt;

    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret;
    if (read) ret = svcIoctlv(fd, 0x0F, 1, 2, iovec);
    else
        ret = svcIoctlv(fd, 0x10, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}

int FSA_ReadFile(int fd, void *data, u32 size, u32 cnt, int fileHandle, u32 flags) {
    return _FSA_ReadWriteFileWithPos(fd, data, size, cnt, 0, fileHandle, flags, true);
}

int FSA_WriteFile(int fd, void *data, u32 size, u32 cnt, int fileHandle, u32 flags) {
    return _FSA_ReadWriteFileWithPos(fd, data, size, cnt, 0, fileHandle, flags, false);
}

int FSA_ReadFileWithPos(int fd, void *data, u32 size, u32 cnt, u32 position, int fileHandle, u32 flags) {
    return _FSA_ReadWriteFileWithPos(fd, data, size, cnt, position, fileHandle, flags, true);
}

int FSA_WriteFileWithPos(int fd, void *data, u32 size, u32 cnt, u32 position, int fileHandle, u32 flags) {
    return _FSA_ReadWriteFileWithPos(fd, data, size, cnt, position, fileHandle, flags, false);
}

int FSA_AppendFile(int fd, u32 size, u32 cnt, int fileHandle) {
    return FSA_AppendFileEx(fd, size, cnt, fileHandle, 0);
}

// flags:
//  - 1: affects the way the blocks are allocated
//  - 2: maybe will cause it to allocate it at the end of the quota?
int FSA_AppendFileEx(int fd, u32 size, u32 cnt, int fileHandle, u32 flags) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    inbuf[1] = size;
    inbuf[2] = cnt;
    inbuf[3] = fileHandle;
    inbuf[4] = flags;

    int ret = svcIoctl(fd, 0x19, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}


int FSA_GetStatFile(int fd, int handle, FSStat *out_data) {
    return dispatch_ioctl_out(fd, 0x14, handle, (u32 *) out_data, sizeof(FSStat));
}

int FSA_CloseFile(int fd, int fileHandle) {
    return dispatch_ioctl(fd, 0x15, fileHandle);
}

int FSA_FlushFile(int fd, int fileHandle) {
    return dispatch_ioctl(fd, 0x17, fileHandle);
}

int FSA_TruncateFile(int fd, int fileHandle) {
    return dispatch_ioctl(fd, 0x1A, fileHandle);
}

int FSA_GetPosFile(int fd, int fileHandle, u32 *out_position) {
    return dispatch_ioctl_out(fd, 0x11, fileHandle, (u32 *) out_position, sizeof(u32));
}

int FSA_SetPosFile(int fd, int fileHandle, u32 position) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    inbuf[1] = fileHandle;
    inbuf[2] = position;

    int ret = svcIoctl(fd, 0x12, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_IsEof(int fd, int fileHandle) {
    return dispatch_ioctl(fd, 0x13, fileHandle);
}

int FSA_Remove(int fd, char *path) {
    return dispatch_ioctl(fd, 0x08, path);
}

int FSA_Rename(int fd, char *old_path, char *new_path) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    strncpy((char *) &inbuf[0x01], old_path, 0x27F);
    strncpy((char *) &inbuf[0x01 + (0x280 / 4)], new_path, 0x27F);

    int ret = svcIoctl(fd, 0x09, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_ChangeMode(int fd, char *path, int mode) {
    return dispatch_ioctl(fd, 0x20, path, mode, 0x777);
}

int FSA_ChangeModeEx(int fd, char *path, int mode, int mask) {
    return dispatch_ioctl(fd, 0x20, path, mode, mask);
}

int FSA_ChangeOwner(int fd, char *path, u32 owner, u32 group) {
    u8 *iobuf   = allocIobuf();
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) &iobuf[0x520];

    strncpy((char *) &inbuf[0x01], path, 0x27F);
    inbuf[0x284 / 4] = 0; // ignored
    inbuf[0x288 / 4] = owner;
    inbuf[0x28C / 4] = 0; // ignored
    inbuf[0x290 / 4] = group;

    int ret = svcIoctl(fd, 0x70, inbuf, 0x520, outbuf, 0x293);

    freeIobuf(iobuf);
    return ret;
}

int FSA_RawOpen(int fd, char *device_path, int *outHandle) {
    return dispatch_ioctl_out(fd, 0x6A, device_path, (u32 *) outHandle, sizeof(int));
}

int _FSA_RawReadWrite(int fd, void *data, u32 size_bytes, u32 cnt, u64 blocks_offset, int device_handle, bool read) {
    u8 *iobuf      = allocIobuf();
    u32 *inbuf     = (u32 *) iobuf;
    u32 *outbuf    = (u32 *) &iobuf[0x520];
    iovec_s *iovec = (iovec_s *) &iobuf[0x7C0];

    inbuf[0x08 / 4] = (blocks_offset >> 32);
    inbuf[0x0C / 4] = (blocks_offset & 0xFFFFFFFF);
    inbuf[0x10 / 4] = cnt;
    inbuf[0x14 / 4] = size_bytes;
    inbuf[0x18 / 4] = device_handle;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x520;

    iovec[1].ptr = data;
    iovec[1].len = size_bytes * cnt;

    iovec[2].ptr = outbuf;
    iovec[2].len = 0x293;

    int ret;
    if (read) ret = svcIoctlv(fd, 0x6B, 1, 2, iovec);
    else
        ret = svcIoctlv(fd, 0x6C, 2, 1, iovec);

    freeIobuf(iobuf);
    return ret;
}

// offset in blocks of 0x1000 bytes
int FSA_RawRead(int fd, void *data, u32 size_bytes, u32 cnt, u64 blocks_offset, int device_handle) {
    return _FSA_RawReadWrite(fd, data, size_bytes, cnt, blocks_offset, device_handle, true);
}

int FSA_RawWrite(int fd, void *data, u32 size_bytes, u32 cnt, u64 blocks_offset, int device_handle) {
    return _FSA_RawReadWrite(fd, data, size_bytes, cnt, blocks_offset, device_handle, false);
}

int FSA_RawClose(int fd, int device_handle) {
    return dispatch_ioctl(fd, 0x6D, device_handle);
}
