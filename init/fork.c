#include <stdio.h>
#include "manpage.h"

static const manpage_entry_t man = {
	.name = "fork",
	.desc = "This function create a new process. The new process (child) is an \
copy of the calling process (parent).",
	.usage = "fork"
};

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }
	printf("This is a second process\n");
	return 0;
}
