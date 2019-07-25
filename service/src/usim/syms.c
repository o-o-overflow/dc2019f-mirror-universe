// syms.c --- routines for handling CADRLP symbol tables

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <err.h>

#include <sys/queue.h>

#include "usim.h"
#include "syms.h"

static void
sym_add(symtab_t *tab, int memory, char *name, uint32_t v)
{
	sym_t *s;

	s = (sym_t *) malloc(sizeof(sym_t));
	if (s == NULL) {
		perror("malloc");
		exit(1);
	}

	s->name = strdup(name);
	s->v = v;
	s->mtype = memory;

	if (LIST_EMPTY(&tab->symbols))
		LIST_INSERT_HEAD(&tab->symbols, s, next);
	else {
		sym_t *ss;
		sym_t *p;

		p = LIST_FIRST(&tab->symbols);
		if (p->v >= v) {
			LIST_INSERT_HEAD(&tab->symbols, s, next);
		} else {
			LIST_FOREACH(ss, &tab->symbols, next) {
				if (ss->v < v)
					p = ss;
			}
			LIST_INSERT_AFTER(p, s, next);
		}
	}

	tab->sym_count++;
}

char *
sym_find_by_type_val(symtab_t *tab, symtype_t memory, uint32_t v, int *offset)
{
	sym_t *s;
	sym_t *closest;

	closest = NULL;
	LIST_FOREACH(s, &tab->symbols, next) {
		if (s->mtype != memory)
			continue;

		/* Found exact match? */
		if (s->v == v) {
			if (offset)
				*offset = 0;
			return s->name;
		} else if (s->v < v) {
			closest = s;
		} else if (s->v > v) {
			break;
		}

	}

	if (closest && offset) {
		*offset = v - closest->v;
		return closest->name;
	}

	return NULL;
}

int
sym_find(symtab_t *tab, char *name, int *pval)
{
	sym_t *s;

	LIST_FOREACH(s, &tab->symbols, next) {
		if (strcasecmp(name, s->name) == 0) {
			*pval = s->v;
			return 0;
		}
	}

	return -1;
}

// Read a CADR MCR symbol file.
int
sym_read_file(symtab_t *tab, char *filename)
{
	FILE *f;
	char line[8 * 1024];

	f = fopen(filename, "r");
	if (f == NULL) {
		warn("failed to open: %s", filename);
		return -1;
	}

	LIST_INIT(&tab->symbols);

	tab->name = strdup(filename);

	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);
	fgets(line, sizeof(line), f);

	while (fgets(line, sizeof(line), f) != NULL) {
		char sym[64];
		char symtype[64];
		int loc;
		int n;

		n = sscanf(line, "%s %s %o", sym, symtype, &loc);
		if (n == 3) {
			int type = 0;

			if (strcmp(symtype, "I-MEM") == 0)
				type = 1;
			else if (strcmp(symtype, "D-MEM") == 0)
				type = 2;
			else if (strcmp(symtype, "A-MEM") == 0)
				type = 4;
			else if (strcmp(symtype, "M-MEM") == 0)
				type = 5;
			else if (strcmp(symtype, "NUMBER") == 0)
				type = 6;
			else
				warnx("unknown section type in symbol table: %s", symtype);

			sym_add(tab, type, sym, loc);
		}
	}

	fclose(f);

	return 0;
}
