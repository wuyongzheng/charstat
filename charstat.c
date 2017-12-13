#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned long stats[65536]; // for plane 0
static unsigned long stats_uplane[17]; // [1-16]: plane 1-16, [0]: above plane 16

/* 0       -> 0
 * 1-9     -> 1
 * 10-99   -> 2
 * 100-999 -> 3
 * ... */
static unsigned int count_digits (unsigned long n)
{
	unsigned int i;
	unsigned long s;
	for (i = 0, s = 1; s * 10 > s; i ++, s *= 10)
		if (n < s)
			return i;
	return i + 1;
}

int decode_utf8 (const unsigned char *instr, unsigned int *punival)
{
	const unsigned char firstchar = instr[0];
	if (firstchar < 0x80) {
		*punival = firstchar;
		return 1;
	} else if (firstchar < 0xc0) {
		return 0;
	} else if (firstchar < 0xe0) {
		if ((instr[1] & 0xc0) != 0x80)
			return 0;
		*punival = ((firstchar & 0x1f) << 6) | (instr[1] & 0x3f);
		return 2;
	} else if (firstchar < 0xf0) {
		if ((instr[1] & 0xc0) != 0x80 || (instr[2] & 0xc0) != 0x80)
			return 0;
		*punival = ((firstchar & 0x0f) << 12) |
			((instr[1] & 0x3f) << 6) |
			(instr[2] & 0x3f);
		return 3;
	} else if (firstchar < 0xf8) {
		if ((instr[1] & 0xc0) != 0x80 || (instr[2] & 0xc0) != 0x80 ||
				(instr[3] & 0xc0) != 0x80)
			return 0;
		*punival = ((firstchar & 0x07) << 18) |
			((instr[1] & 0x3f) << 12) |
			((instr[2] & 0x3f) << 6) |
			(instr[3] & 0x3f);
		return 4;
	}
	return 0;
}

/* given a unicode character "unival", generate the UTF-8
 * string in "outstr", return the length of it.
 * The output string is NOT null terminated. */
int encode_utf8 (unsigned char *outstr, unsigned int unival)
{
	if (unival < 0x80) {
		outstr[0] = unival;
		return 1;
	}
	if (unival < 0x800) {
		outstr[0] = 0xc0 | (unival >> 6);
		outstr[1] = 0x80 | (unival & 0x3f);
		return 2;
	}
	if (unival < 0x10000) {
		outstr[0] = 0xe0 | (unival >> 12);
		outstr[1] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[2] = 0x80 | (unival & 0x3f);
		return 3;
	}
	if (unival < 0x110000) {
		outstr[0] = 0xf0 | (unival >> 18);
		outstr[1] = 0x80 | ((unival >> 12) & 0x3f);
		outstr[2] = 0x80 | ((unival >> 6) & 0x3f);
		outstr[3] = 0x80 | (unival & 0x3f);
		return 4;
	}
	return 0;
}

static void process_file_utf8 (FILE *fp)
{
	unsigned char buffer[8192 + 6]; // extra guard bytes preventing decoder over-shoot

	memset(stats, 0, sizeof(stats));
	memset(stats_uplane, 0, sizeof(stats_uplane));
	memset(buffer, 0, sizeof(buffer));

	size_t length = 0;
	while (1) {
		size_t read_length = fread(buffer + length, 1, 8192 - length, fp);
		if (read_length == 0) {
			if (length != 0)
				fprintf(stderr, "remain undecoded code\n");
			break;
		}
		length += read_length;
		buffer[length] = 0;

		size_t i = 0;
		while (i < length) {
			unsigned int unival;
			int advance = decode_utf8(buffer + i, &unival);
			if (advance > 0) {
				if (unival < 65536)
					stats[unival] ++;
				else if (unival < 17 * 65536)
					stats_uplane[unival / 65536] ++;
				else
					stats_uplane[0] ++;

				i += advance;
				assert(i <= length);
				continue;
			}
			assert(advance == 0);
			if (length - i >= 6) {
				fprintf(stderr, "invalid utf8 code\n");
				i ++;
				continue;
			} else { // do the remaining in the next fileread.
				break;
			}
		}

		assert(length >= i && length - i < 6);
		memcpy(buffer, buffer + i, length - i);
		length = length - i;
	}
}

static void print_stats_utf8 (void)
{
	int i;
	for (i = 0; i < 65536; i ++) {
		if (stats[i]) {
			char str[100];
			int ptr = 0;
			ptr += sprintf(str + ptr, "%d\t", i);
			if (i <= 0x20 || i == 0x7f)
				ptr += sprintf(str + ptr, ".\t");
			else {
				ptr += encode_utf8((unsigned char *)str + ptr, i);
				str[ptr ++] = '\t';
			}
			ptr += sprintf(str + ptr, "%lu", stats[i]);
			str[ptr] = '\0';
			puts(str);
		}
	}
	for (i = 1; i <= 16; i ++) {
		if (stats_uplane[i]) {
			printf("plane %d\t%lu\n", i, stats_uplane[i]);
		}
	}
	if (stats_uplane[0])
		printf("above\t%lu\n", stats_uplane[0]);
}

static void process_file_byte (FILE *fp)
{
	unsigned char buffer[8192];
	size_t length;

	memset(stats, 0, sizeof(stats));
	while ((length = fread(buffer, 1, 8192, fp)) > 0) {
		int i;
		for (i = 0; i < length; i ++)
			stats[buffer[i]] ++;
	}
}

static void print_stats_byte (void)
{
	int maxdigits[8];
	int row, col;

	for (col = 0; col < 8; col ++) {
		maxdigits[col] = 0;
		for (row = 0; row < 32; row ++) {
			if (maxdigits[col] < count_digits(stats[col * 32 + row]))
				maxdigits[col] = count_digits(stats[col * 32 + row]);
		}
	}

	for (row = 0; row < 32; row ++) {
		for (col = 0; col < 8; col ++) {
			int c = col * 32 + row;

			if (maxdigits[col] == 0)
				continue;

			if (c >= ' ' && c <= '~')
				printf("%c", c);
			else if (c == 0x7f)
				printf(" ");
			else
				printf("%02x", c);

			if (stats[c] == 0) {
				int i;
				putchar(':');
				for (i = 0; i <= maxdigits[col]; i ++)
					putchar(' ');
			} else {
				char format[10];
				sprintf(format, ":%%-%dlu ", maxdigits[col]);
				printf(format, stats[c]);
			}
		}
		printf("\n");
	}
}

static void usage (const char *exe)
{
}

int main (int argc, char **argv)
{
	char *infile = NULL;
	int encoding = 0; // 0: raw byte; 1: utf8

	if (argc == 1) {
	} else if (argc == 2) {
		if (strcmp(argv[1], "-u") == 0) {
			encoding = 1;
		} else if (strcmp(argv[1], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else {
			infile = argv[1];
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "-u") != 0) {
			usage(argv[0]);
			return 1;
		} else {
			encoding = 1;
			infile = argv[2];
		}
	}

	FILE *infp = infile == NULL ? stdin : fopen(infile, "rb");
	if (infp == NULL) {
		perror(argv[0]);
		return 1;
	}
	if (encoding == 0)
		process_file_byte(infp);
	else
		process_file_utf8(infp);
	if (infile != NULL)
		fclose(infp);

	if (encoding == 0)
		print_stats_byte();
	else
		print_stats_utf8();

	return 0;
}
