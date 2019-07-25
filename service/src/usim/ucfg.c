// ucfg.c --- configuration handling

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <err.h>

#include "ini.h"

#include "ucfg.h"
#include "utrace.h"
#include "kbd.h"
#include "chaos.h"

#include "misc.h"

ucfg_t ucfg = {
#define X(s, n, default) default,
#include "ucfg.defs"
#undef X
};

#define INIHEQ(s, n) (streq(s, section) && streq(n, name))

int
ucfg_handler(void *user, const char *section, const char *name, const char *value)
{
	ucfg_t *cfg = (ucfg_t *) user;

	if (0) ;
#define X(s, n, default)					\
	else if (INIHEQ(#s, #n)) cfg->s##_##n = strdup(value);
#include "ucfg.defs"
#undef X

	if (INIHEQ("chaos", "myaddr")) {
		int addr;
		char *end;

		addr = strtoul(value, &end, 8);
		if (*end != 0 || addr > 0177777)
			errx(1, "chaosnet address must be a 16-bit octal number");
		chaos_set_addr(addr);
	}

	if (INIHEQ("trace", "level")) {
		     if (streq(cfg->trace_level, "alert"))   trace_level = LOG_ALERT;
		else if (streq(cfg->trace_level, "crit"))    trace_level = LOG_CRIT;
		else if (streq(cfg->trace_level, "debug"))   trace_level = LOG_DEBUG;
		else if (streq(cfg->trace_level, "emerg"))   trace_level = LOG_EMERG;
		else if (streq(cfg->trace_level, "err"))     trace_level = LOG_ERR;
		else if (streq(cfg->trace_level, "info"))    trace_level = LOG_INFO;
		else if (streq(cfg->trace_level, "notice"))  trace_level = LOG_NOTICE;
		else if (streq(cfg->trace_level, "warning")) trace_level = LOG_WARNING;
		else warnx("unknown trace level: %s", cfg->trace_level);
	}

	if (INIHEQ("trace", "facilities")) {
		char *s;
		char *sp;

		s = strdup(cfg->trace_facilities);
		sp = strtok(s, " ");
		while (sp != NULL) {
			     if (streq(sp, "all"))   trace_facilities = TRACE_ALL;
			else if (streq(sp, "none"))  trace_facilities = TRACE_NONE;
			else if (streq(sp, "misc"))  trace_facilities |= TRACE_MISC;
			else if (streq(sp, "vm"))    trace_facilities |= TRACE_VM;
			else if (streq(sp, "int"))   trace_facilities |= TRACE_INT;
			else if (streq(sp, "disk"))  trace_facilities |= TRACE_DISK;
			else if (streq(sp, "chaos")) trace_facilities |= TRACE_CHAOS;
			else if (streq(sp, "iob"))   trace_facilities |= TRACE_IOB;
			else if (streq(sp, "microcode")) trace_facilities |= TRACE_MICROCODE;
			else if (streq(sp, "macrocode")) trace_facilities |= TRACE_MACROCODE;
			else warnx("unknown trace facility: %s", sp);

			sp = strtok(NULL, " ");
		}

		free(s);
	}

	if (INIHEQ("kbd", "type")) {
		     if (streq("knight", cfg->kbd_type)) kbd_type = 0;
		else if (streq("cadet", cfg->kbd_type))  kbd_type = 1;
		else warnx("unknown keyboard type: %s", cfg->kbd_type);
	}

	return 1;
}
