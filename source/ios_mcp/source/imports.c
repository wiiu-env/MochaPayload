#include "imports.h"

int strlen(const char *str) {
    unsigned int i = 0;
    while (str[i]) {
        i++;
    }
    return i;
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

char *strchr(const char *p, int ch) {
    char c;

    c = ch;
    for (;; ++p) {
        if (*p == c)
            return ((char *) p);
        if (*p == '\0')
            return (NULL);
    }
    /* NOTREACHED */
}

char *
strrchr(s, c)
register const char *s;
int c;
{
    char *rtnval = 0;

    do {
        if (*s == c)
            rtnval = (char *) s;
    } while (*s++);
    return (rtnval);
}
