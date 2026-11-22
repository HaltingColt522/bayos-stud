#include <unistd.h>
#include "manpage.h"

static const manpage_entry_t man = {
	.name = "clear",
	.desc = "Clears the terminal.",
	.usage = "clear"
};

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }
	write(1, "\033[2J\033[H", 7);
	return 0;
}
