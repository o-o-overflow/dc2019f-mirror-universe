#ifndef USIM_DISASS_H
#define USIM_DISASS_H

#include "syms.h"

extern char *uinst_desc(uint64_t u, symtab_t *symtab);
extern char *disassemble_instruction(uint32_t fef, uint32_t pc, uint32_t wd, uint32_t second_word);

#endif
