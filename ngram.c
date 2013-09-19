#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* returns the number of bytes of the utf8 character.
 * if this byte is not the first byte, return 0. */
static int utf8_len (unsigned char utfchar)
{
	if (utfchar < 0x80)
		return 1;
	else if (utfchar < 0xc0)
		return 0;
	else if (utfchar < 0xe0)
		return 2;
	else if (utfchar < 0xf0)
		return 3;
	else if (utfchar < 0xf8)
		return 4;
	else
		return 0;
}

int main (int argc, char *argv[])
{
	if (argc != 2 || argv[1][0] < '0' || argv[1][0] > '9') {
		fprintf(stderr, "Usage: %s N\n", argv[0]);
		fprintf(stderr, "Eg:    %s 3\n", argv[0]);
		return 1;
	}
	int ngramn = atoi(argv[1]);
	assert(ngramn >= 1);

	while (1) {
		char linebuf[16000];
		int linelen;
		if (fgets(linebuf, sizeof(linebuf), stdin) == NULL)
			break;
		linelen = strlen(linebuf);
		while (linebuf[linelen - 1] == '\r' || linebuf[linelen - 1] == '\n')
			linelen --;
		if (linelen < ngramn)
			continue;
		linebuf[linelen] = '\0';

		int utfindex[ngramn];
		utfindex[0] = 0;
		int utfptr = 0;
		int ptr = 0;
		while (ptr < linelen) {
			int jump = utf8_len((unsigned char)linebuf[ptr]);
			if (jump == 0)
				jump = 1;
			ptr += jump;
			utfptr ++;
			if (utfptr >= ngramn) {
				fwrite(linebuf + utfindex[utfptr % ngramn], ptr - utfindex[utfptr % ngramn], 1, stdout);
				putchar('\n');
			}
			utfindex[utfptr % ngramn] = ptr;
		}
	}

	return 0;
}
