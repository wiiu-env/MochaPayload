#include "imports.h"

void usleep(u32 time) {
    ((void (*const)(u32)) 0x050564E4)(time);
}

void *memset(void *dst, int val, size_t size) {
    char *_dst = dst;

    int i;
    for (i = 0; i < size; i++)
        _dst[i] = val;

    return dst;
}

void *(*const _memcpy)(void *dst, void *src, int size) = (void *) 0x05054E54;

void *memcpy(void *dst, const void *src, size_t size) {
    return _memcpy(dst, (void *) src, size);
}

int strlen(const char *str) {
    unsigned int i = 0;
    while (str[i]) {
        i++;
    }
    return i;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    } else {
        return (*(unsigned char *) s1 - *(unsigned char *) s2);
    }
}

// Function to implement strncat() function in C
char *strncat(char *destination, const char *source, size_t num) {
    // make ptr point to the end of destination string
    char *ptr = destination + strlen(destination);

    // Appends characters of source to the destination string
    while (*source != '\0' && num--)
        *ptr++ = *source++;

    // null terminate destination string
    *ptr = '\0';

    // destination string is returned by standard strncat()
    return destination;
}

char *strncpy(char *dst, const char *src, size_t size) {
    int i;
    for (i = 0; i < size; i++) {
        dst[i] = src[i];
        if (src[i] == '\0')
            return dst;
    }

    return dst;
}

int vsnprintf(char *s, size_t n, const char *format, va_list arg) {
    return ((int (*const)(char *, size_t, const char *, va_list)) 0x05055C40)(s, n, format, arg);
}
