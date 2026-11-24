#ifndef GDT_H
#define GDT_H

#include <stdint.h>

int8_t gdt_init(void);
void change_rsp0(void*);

#endif
