#ifndef INITRAMFS_H
#define INITRAMFS_H

#include <stdint.h>

int initramfs_lookup(char *filename, uint8_t **out);

#endif // INITRAMFS_H
