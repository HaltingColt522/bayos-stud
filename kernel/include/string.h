#ifndef STRING_H
#define STRING_H

static inline size_t strlen(const char *s) {
	size_t len = 0;

	while (s[len] != '\0') {
		++len;
	}

	return len;
}

static inline char *strchr(const char *s, int c) {
	unsigned char target = (unsigned char)c;

	for (;;) {
		if ((unsigned char) * s == target) {
			return (char *)s;
		}

		if (*s == '\0') {
			return NULL;
		}

		++s;
	}
}

#endif // STRING_H
