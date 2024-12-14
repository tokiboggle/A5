#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdint.h>

int32_t sign_extend(uint32_t value, int bits);
uint32_t get_bits(uint32_t insn, int start, int len);
const char* get_opcode_name(uint32_t insn); // Declare this function

#endif