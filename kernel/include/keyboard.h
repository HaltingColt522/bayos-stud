#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <pipe.h>

void keyboard_init(void);

extern pipe_t kb_pipe;
extern void keyboard_int_handler(void);

#endif // !KEYBOARD_H
