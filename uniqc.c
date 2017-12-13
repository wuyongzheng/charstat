#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* on 32 bit system, they are the same.
 * on 64 bit system, seems "int" run faster, at least when lines ash short
 * I still keep "long" as an ugly compile time option. */
typedef unsigned int hash_t; // more collision, more compact htable, faster hashing
//typedef unsigned long hash_t; // less collision, larger htable, slower hashing
typedef unsigned int count_t;
//typedef unsigned long count_t;

struct ht_entry {
	hash_t hashval;
	count_t count; // make sure they align correctly
	char *str;
};

static struct ht_entry *ht_table = NULL;
static unsigned int ht_size;
static unsigned int ht_used;

#define SORT_NONE 0
#define SORT_TEXT 1
#define SORT_COUNT 2
static int option_sortby = SORT_NONE;
static int option_descending = 0;
#define PRINTORDER_CT 0
#define PRINTORDER_TC 1
#define PRINTORDER_T  2
static int option_printorder = PRINTORDER_CT;
static int option_width = 1;
static char option_tab = '\t';

static char *my_strdup (const char *s)
{
	const int POLLSIZE = 16384, MAXPOOLED = 16;
	static char *pool = NULL;
	static int polllen = 0;

	int len = strlen(s) + 1;
	if (len == 1)
		return (char *)"";
	if (len > MAXPOOLED)
		return strdup(s);
	if (pool == NULL || polllen + len > POLLSIZE) {
		pool = malloc(POLLSIZE);
		polllen = 0;
	}
	memcpy(pool + polllen, s, len);
	polllen += len;
	return pool + polllen - len;
}

static hash_t ht_calhash (const char *str)
{
	hash_t hashval = sizeof(hash_t) == 8 ? 14695981039346656037ul : 2166136261ul;
	while (*str)
		hashval = (hashval * (sizeof(hash_t) == 8 ? 1099511628211ul : 16777619ul)) ^
			(*(const unsigned char *)(str ++));
	return hashval;
}

static void ht_create (void)
{
	assert(ht_table == NULL);
	ht_size = 1031;
	ht_used = 0;
	ht_table = (struct ht_entry *)calloc(ht_size, sizeof(struct ht_entry));
}

static int next_prime (int i)
{
	static const int smallprime [] = {
		3,5,7,11,13,17,19,23,29,31,
		37,41,43,47,53,59,61,67,71,
		73,79,83,89,97,0};
	assert(i > 0);
	if (i % 2 == 0)
		i ++;
	for (; ; i += 2) {
		int j;
		for (j = 0; smallprime[j] && i % smallprime[j] != 0; j ++)
			;
		if (smallprime[j]) {
			if (i == smallprime[j])
				return i;
			continue;
		}
		for (j = 101; j*j < i; j ++)
			if (i % j == 0)
				break;
		if (j*j > i)
			return i;
	}
}

static void ht_add (const char *str)
{
	if (ht_used * 4 > ht_size * 3) {
		/*{ // hashtable statistics
			int i, j, maxj = 0;
			long sqrsum = 0;
			for (i = j = 0; i < ht_size; i ++) {
				if (ht_table[i].count != 0)
					j ++;
				else
					j = 0;
				if (maxj < j)
					maxj = j;
				sqrsum += j;
			}
			printf("htstat: used=%d, size=%d, sqrsum=%ld, max=%d\n", ht_used, ht_size, sqrsum, maxj);
		}*/
		unsigned int new_size = next_prime(ht_size * 2);
		struct ht_entry *new_table = (struct ht_entry *)calloc(new_size, sizeof(struct ht_entry));
		unsigned int i;
		for (i = 0; i < ht_size; i ++) {
			if (ht_table[i].count == 0)
				continue;
			int j;
			for (j = ht_table[i].hashval % new_size; new_table[j].count != 0; j = (j + 1) % new_size)
				;
			new_table[j] = ht_table[i];
		}
		free(ht_table);
		ht_table = new_table;
		ht_size = new_size;
	}

	hash_t hashval = ht_calhash(str);
	unsigned int i;
	for (i = hashval % ht_size;
			ht_table[i].count != 0 &&
				(ht_table[i].hashval != hashval ||
					strcmp(ht_table[i].str, str) != 0);
			i = (i + 1) % ht_size)
		;

	if (ht_table[i].count == 0) {
		ht_table[i].hashval = hashval;
		ht_table[i].str = my_strdup(str);
		ht_table[i].count = 1;
		ht_used ++;
	} else {
		ht_table[i].count ++;
	}
}

static void process_file (FILE *fp)
{
	char buffer[65536];
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		int len = strlen(buffer);
		while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
			buffer[-- len] = '\0';
		ht_add(buffer);
	}
}

static int compare_text (const void *entry1, const void *entry2)
{
	return strcmp((*(struct ht_entry **)entry1)->str, (*(struct ht_entry **)entry2)->str);
}

static int compare_count (const void *entry1, const void *entry2)
{
	return (*(struct ht_entry **)entry1)->count - (*(struct ht_entry **)entry2)->count;
}

static void print_entry (const char *str, count_t count)
{
	static char format [100];
	static int formatted = 0;
	if (formatted == 0) {
		switch (option_printorder) {
		case PRINTORDER_CT: snprintf(format, sizeof(format), "%%0%dd%%c%%s\n", option_width); break;
		case PRINTORDER_TC: snprintf(format, sizeof(format), "%%s%%c%%0%dd\n", option_width); break;
		}
		formatted = 1;
	}

	switch (option_printorder) {
	case PRINTORDER_CT: printf(format, count, option_tab, str); break;
	case PRINTORDER_TC: printf(format, str, option_tab, count); break;
	case PRINTORDER_T: puts(str); break;
	default: assert(0);
	}
}

static void usage (const char *cmd)
{
	printf("Usage: %s [opts] [files ...] \n", cmd);
	puts("  -c: sort by count (default: no sort)");
	puts("  -t: sort by text (default: no sort)");
	puts("  -d: sort in desending order (default: ascending order)");
	puts("  -r: text followed by count (default: count followed by text)");
	puts("  -u: do not print count (default: count followed by text)");
	puts("  -w n: print count in fixed n digit. prepend with 0");
	puts("  -f c: use character c as the delimiter. (default: TAB)");
}

int main (int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "ctdruw:f:")) != -1) {
		switch (opt) {
		case 'c': option_sortby = SORT_COUNT; break;
		case 't': option_sortby = SORT_TEXT; break;
		case 'd': option_descending = 1; break;
		case 'r': option_printorder = PRINTORDER_TC; break;
		case 'u': option_printorder = PRINTORDER_T; break;
		case 'w':
			option_width = atoi(optarg);
			if (option_width < 0)
				option_width = 0;
			break;
		case 'f': option_tab = optarg[0]; break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	ht_create();
	if (optind >= argc) {
		process_file(stdin);
	} else {
		for (opt = optind; opt < argc; opt ++) {
			FILE *fp = strcmp(argv[opt], "-") == 0 ? stdin : fopen(argv[opt], "r");
			if (fp == NULL) {
				perror(argv[opt]);
			} else {
				process_file(fp);
				fclose(fp);
			}
		}
	}

	if (ht_used == 0)
		return 0;

	if (option_sortby == SORT_NONE) {
		int i;
		for (i = 0; i < ht_size; i ++) {
			if (ht_table[i].count)
				print_entry(ht_table[i].str, ht_table[i].count);
		}
	} else {
		struct ht_entry **sorted = (struct ht_entry **)malloc(ht_used * sizeof(struct ht_entry *));
		int i, j;
		for (i = j = 0; i < ht_size && j < ht_used; i ++)
			if (ht_table[i].count != 0)
				sorted[j ++] = &ht_table[i];

		qsort(sorted, ht_used, sizeof(struct ht_entry *), option_sortby == SORT_TEXT ? compare_text : compare_count);
		if (option_descending) {
			for (i = ht_used - 1; i >= 0; i --)
				print_entry(sorted[i]->str, sorted[i]->count);
		} else {
			for (i = 0; i < ht_used; i ++)
				print_entry(sorted[i]->str, sorted[i]->count);
		}
	}

	return 0;
}
