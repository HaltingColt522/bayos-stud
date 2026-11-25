#include <stdio.h>
#include <string.h>
#include "manpage.h"

#define WRAP     40
#define MAXTEXT  4096
#define MAXLINES 128

static const manpage_entry_t man = {
	.name = "cowsay",
	.desc = "Displays a little cow with the given text. When no argument is parsed,\
the program will wait for the input.",
	.usage = "cowsay [text]"
};

static const char *cow =
    "        \\   ^__^\n"
    "         \\  (oo)\\_______\n"
    "            (__)\\       )\\/\\\n"
    "                ||----w |\n"
    "                ||     ||\n";

static char text[MAXTEXT];
static char lines[MAXLINES][WRAP + 1];
static int nlines;

static void push(const char *s, int n) {
	if (nlines >= MAXLINES) { return; }
	memcpy(lines[nlines], s, n);
	lines[nlines][n] = 0;
	nlines++;
}

static int is_space(char c) { return c == ' ' || c == '\n' || c == '\t'; }

static void wrap(void) {
	char cur[WRAP + 1];
	int len = 0;
	char *p = text;

	while (*p) {
		while (is_space(*p)) { p++; }
		if (!*p) { break; }

		char *w = p;
		while (*p && !is_space(*p)) { p++; }
		int wl = (int)(p - w);

		while (wl > WRAP) {              /* zu langes Wort hart trennen */
			if (len) { push(cur, len); len = 0; }
			push(w, WRAP);
			w += WRAP;
			wl -= WRAP;
		}
		if (len && len + 1 + wl > WRAP) { push(cur, len); len = 0; }
		if (len) { cur[len++] = ' '; }
		memcpy(cur + len, w, wl);
		len += wl;
	}
	if (len) { push(cur, len); }
	if (nlines == 0) { push("", 0); }
}

int main(int argc, char **argv) {
	if (handle_manpage(&man, argc, argv)) { return 0; }
	if (argc > 1) {
		size_t pos = 0;
		for (int i = 1; i < argc && pos < MAXTEXT - 2; i++) {
			size_t l = strlen(argv[i]);
			if (pos + l + 1 >= MAXTEXT) { break; }
			memcpy(text + pos, argv[i], l);
			pos += l;
			text[pos++] = ' ';
		}
		text[pos] = 0;
	} else if (!fgets(text, MAXTEXT, stdin)) {
		text[0] = 0;
	}

	wrap();

	int width = 0;
	for (int i = 0; i < nlines; i++) {
		int l = (int)strlen(lines[i]);
		if (l > width) { width = l; }
	}

	putchar(' ');
	for (int i = 0; i < width + 2; i++) { putchar('_'); }
	putchar('\n');

	for (int i = 0; i < nlines; i++) {
		char l = '|', r = '|';
		if (nlines == 1)          { l = '<'; r = '>'; }
		else if (i == 0)          { l = '/'; r = '\\'; }
		else if (i == nlines - 1) { l = '\\'; r = '/'; }

		printf("%c %s", l, lines[i]);
		for (int k = (int)strlen(lines[i]); k < width; k++) { putchar(' '); }
		printf(" %c\n", r);
	}

	putchar(' ');
	for (int i = 0; i < width + 2; i++) { putchar('-'); }
	putchar('\n');

	fputs(cow, stdout);
	return 0;
}
