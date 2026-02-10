#include "socket.h"
#include "imports.h"
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int socket_handle = 0;

int socketInit() {
    if (socket_handle) return socket_handle;

    int ret = svcOpen("/dev/socket", 0);

    if (ret > 0) {
        socket_handle = ret;
        return socket_handle;
    }

    return ret;
}

int socketExit() {
    int ret = svcClose(socket_handle);

    socket_handle = 0;

    return ret;
}

static void *allocIobuf(u32 size) {
    void *ptr = svcAlloc(0xCAFF, size);

    if (ptr) memset(ptr, 0x00, size);

    return ptr;
}

static void freeIobuf(void *ptr) {
    svcFree(0xCAFF, ptr);
}

int socket(int domain, int type, int protocol) {
    u8 *iobuf  = allocIobuf(0xC);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = domain;
    inbuf[1] = type;
    inbuf[2] = protocol;

    int ret = svcIoctl(socket_handle, 0x11, inbuf, 0xC, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int closesocket(int sockfd) {
    u8 *iobuf  = allocIobuf(0x4);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = sockfd;

    int ret = svcIoctl(socket_handle, 0x3, inbuf, 0x4, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    u8 *iobuf   = allocIobuf(0x18);
    u32 *inbuf  = (u32 *) iobuf;
    u32 *outbuf = (u32 *) inbuf;

    inbuf[0] = sockfd;

    int ret = -1;

    if (addr && addrlen && *addrlen == 0x10) {
        inbuf[5] = *addrlen;

        ret = svcIoctl(socket_handle, 0x1, inbuf, 0x18, outbuf, 0x18);

        if (ret >= 0) {
            memcpy(addr, &outbuf[1], outbuf[5]);
            *addrlen = outbuf[5];
        }
    } else {
        inbuf[5] = 0x10;

        ret = svcIoctl(socket_handle, 0x1, inbuf, 0x18, outbuf, 0x18);
    }

    freeIobuf(iobuf);
    return ret;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (addrlen != 0x10) return -1;

    u8 *iobuf  = allocIobuf(0x18);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = sockfd;
    memcpy(&inbuf[1], addr, addrlen);
    inbuf[5] = addrlen;

    int ret = svcIoctl(socket_handle, 0x2, inbuf, 0x18, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (addrlen != 0x10) return -1;

    u8 *iobuf  = allocIobuf(0x18);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = sockfd;
    memcpy(&inbuf[1], addr, addrlen);
    inbuf[5] = addrlen;

    int ret = svcIoctl(socket_handle, 0x4, inbuf, 0x18, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int listen(int sockfd, int backlog) {
    u8 *iobuf  = allocIobuf(0x8);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = sockfd;
    inbuf[1] = backlog;

    int ret = svcIoctl(socket_handle, 0xA, inbuf, 0x8, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int shutdown(int sockfd, int how) {
    u8 *iobuf  = allocIobuf(0x8);
    u32 *inbuf = (u32 *) iobuf;

    inbuf[0] = sockfd;
    inbuf[1] = how;

    int ret = svcIoctl(socket_handle, 0x10, inbuf, 0x8, NULL, 0);

    freeIobuf(iobuf);
    return ret;
}

int recv(int sockfd, void *buf, size_t len, int flags) {
    if (!len) return -101;

    // TODO : size checks, split up data into multiple vectors if necessary
    void *data_buf = svcAllocAlign(0xCAFF, len, 0x40);
    if (!data_buf) return -100;

    u8 *iobuf      = allocIobuf(0x38);
    iovec_s *iovec = (iovec_s *) iobuf;
    u32 *inbuf     = (u32 *) &iobuf[0x30];

    inbuf[0] = sockfd;
    inbuf[1] = flags;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x8;
    iovec[1].ptr = (void *) data_buf;
    iovec[1].len = len;

    int ret = svcIoctlv(socket_handle, 0xC, 1, 3, iovec);

    if (ret > 0 && buf) {
        memcpy(buf, data_buf, ret);
    }

    freeIobuf(data_buf);
    freeIobuf(iobuf);
    return ret;
}


static void splitBuffer(void *buf, uint32_t len, void *out1, uint32_t *outSize1, uint32_t *outSizeLeft, void *out2, uint32_t *outSize2, bool write) {
    *outSizeLeft = 0;
    *outSize2    = 0;

    uint32_t size = ((((uint32_t) buf - 1) | 0x3f) + 1) - (uint32_t) buf;
    if (len < size) {
        size = len;
    }

    *outSize1 = size;
    if (write) {
        memcpy(out1, buf, size);
    }

    if (size < len) {
        size              = ((uint32_t) buf + len) & 0x3f;
        uint32_t sizeLeft = len - (*outSize1 - size);

        *outSize2    = size;
        *outSizeLeft = sizeLeft;

        if (write && size != 0) {
            memcpy(out2, (uint8_t *) buf + *outSize1 + sizeLeft, size);
        }
    }
}

static void combineBuffer(void *buf, uint32_t len, void *inBuf1, uint32_t inSize1, uint32_t sizeLeft, void *inBuf2) {
    if (inSize1 != 0) {
        uint32_t size = len;
        if (size > inSize1) {
            size = inSize1;
        }
        memcpy(buf, inBuf1, size);
    }

    uint32_t size = len - (inSize1 + sizeLeft);
    if (size > 0) {
        memcpy((uint8_t *) buf + sizeLeft + inSize1, inBuf2, size);
    }
}


ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    if ((!src_addr && (!addrlen || *addrlen != sizeof(struct sockaddr_in))) || !buf) {
        return -1;
    }
    int res           = -1;
    iovec_s *vecs     = NULL;
    uint8_t *tmp_buf1 = NULL;
    uint8_t *tmp_buf2 = NULL;
    uint8_t *tmp_buf3 = NULL;
    uint8_t *tmp_buf4 = NULL;
    vecs              = (iovec_s *) svcAllocAlign(0xCAFF, sizeof(IOSVec) * 11 + 4, 0x40);
    if (!vecs) {
        res = -100;
        goto exit;
    }

    ((uint32_t *) vecs)[0x20] = sockfd;
    ((uint32_t *) vecs)[0x21] = flags;

    vecs[0].ptr = ((uint32_t *) vecs) + 0x20;
    vecs[0].len = 8;

    vecs[1].len = 0;
    vecs[2].len = 0;
    vecs[3].len = 0;

    tmp_buf1 = (uint8_t *) svcAllocAlign(0xCAFF, 128, 0x40);
    if (!tmp_buf1) {
        res = -101;
        goto exit;
    }
    tmp_buf2 = (uint8_t *) svcAllocAlign(0xCAFF, 128, 0x40);
    if (!tmp_buf2) {
        res = -102;
        goto exit;
    }
    tmp_buf3 = (uint8_t *) svcAllocAlign(0xCAFF, 1600, 0x40);
    if (!tmp_buf3) {
        res = -103;
        goto exit;
    }
    tmp_buf4 = (uint8_t *) svcAllocAlign(0xCAFF, 128, 0x40);
    if (!tmp_buf4) {
        res = -104;
        goto exit;
    }

    // buffer is already aligned correctly
    if ((((uint32_t) buf & 0x3f) == 0) && ((len & 0x3f) == 0)) {
        vecs[1].ptr = buf;
        vecs[1].len = len;
    }
    // buffer is small enough to fit into tmp_buf3
    else if (len < 0x5c7) {
        vecs[1].ptr = tmp_buf3;
        vecs[1].len = len;
    }
    // split buffer over the tmp_bufs
    else {
        splitBuffer(buf, len, tmp_buf1, &vecs[1].len, &vecs[2].len, tmp_buf2, &vecs[3].len, false);
        vecs[1].ptr = tmp_buf1;
        vecs[2].ptr = (uint8_t *) buf + vecs[1].len;
        vecs[3].ptr = tmp_buf2;
    }

    if (!src_addr) {
        vecs[4].ptr = NULL;
        vecs[4].len = 0;
        res         = svcIoctlv(socket_handle, 0xD, 1, 3, vecs);
    } else {
        vecs[4].ptr = tmp_buf4;
        vecs[4].len = *addrlen;
        res         = svcIoctlv(socket_handle, 0xD, 1, 4, vecs);
    }

    if (res >= 0) {
        if ((vecs[1].ptr != buf || len != vecs[1].len)) {
            if (len < 0x5c7) {
                memcpy(buf, tmp_buf3, res);
            } else {
                combineBuffer(buf, res, vecs[1].ptr, vecs[1].len, vecs[2].len, vecs[3].ptr);
            }
        }

        if (src_addr && addrlen) {
            memcpy(src_addr, tmp_buf4, *addrlen);
        }
    }
exit:
    if (vecs) freeIobuf(vecs);
    if (tmp_buf1) freeIobuf(tmp_buf1);
    if (tmp_buf2) freeIobuf(tmp_buf2);
    if (tmp_buf3) freeIobuf(tmp_buf3);
    if (tmp_buf4) freeIobuf(tmp_buf4);
    return res;
}

int send(int sockfd, const void *buf, size_t len, int flags) {
    if (!buf || !len) return -101;

    // TODO : size checks, split up data into multiple vectors if necessary
    void *data_buf = svcAllocAlign(0xCAFF, len, 0x40);
    if (!data_buf) return -100;

    u8 *iobuf      = allocIobuf(0x38);
    iovec_s *iovec = (iovec_s *) iobuf;
    u32 *inbuf     = (u32 *) &iobuf[0x30];

    memcpy(data_buf, buf, len);

    inbuf[0] = sockfd;
    inbuf[1] = flags;

    iovec[0].ptr = inbuf;
    iovec[0].len = 0x8;
    iovec[1].ptr = (void *) data_buf;
    iovec[1].len = len;

    int ret = svcIoctlv(socket_handle, 0xE, 4, 0, iovec);

    freeIobuf(data_buf);
    freeIobuf(iobuf);
    return ret;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    void *data_buf = svcAllocAlign(0xCAFF, optlen, 0x40);
    if (!data_buf) return -100;

    u8 *iobuf      = allocIobuf(sizeof(IOSVec) * 2 + sizeof(uint32_t) * 3);
    iovec_s *iovec = (iovec_s *) iobuf;

    memcpy(data_buf, optval, optlen);
    iovec[0].ptr = data_buf;
    iovec[0].len = optlen;

    iovec[1].ptr = iovec;
    iovec[1].len = sizeof(IOSVec) * 3;

    ((uint32_t *) iovec)[6] = sockfd;
    ((uint32_t *) iovec)[7] = level;
    ((uint32_t *) iovec)[8] = optname;

    int ret = svcIoctlv(socket_handle, 0x9, 2, 0, iovec);
    freeIobuf(iobuf);
    freeIobuf(data_buf);
    return ret;
}
