#ifndef STRING_H
#define STRING_H

static inline size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len] != '\0')
        ++len;

    return len;
}

#endif // STRING_H