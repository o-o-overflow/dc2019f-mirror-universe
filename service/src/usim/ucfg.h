#ifndef USIM_UCFG_H
#define USIM_UCFG_H

#include "ini.h"

typedef struct {
#define X(s, n, default) char *s##_##n;
#include "ucfg.defs"
#undef X
} ucfg_t;

extern ucfg_t ucfg;

extern int ucfg_handler(void *user, const char *section, const char *name, const char *value);

#endif
