#ifndef INITRAMFS_H
#define INITRAMFS_H

#include <stdint.h>
#include <stddef.h>

int initramfs_lookup(char *filename, uint8_t **out);
int initramfs_list_dir(const char *dir, char *buf, size_t buflen);
#endif // INITRAMFS_H
