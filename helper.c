#include "helper.h"

int32_t sign_extend(uint32_t value, int bits) {
    if (value & (1U << (bits - 1))) {
        return (int32_t)value | ~((1U << bits) - 1);
    }
    return (int32_t)value;
}

uint32_t get_bits(uint32_t insn, int start, int len) {
    return (insn >> start) & ((1U << len) - 1);
}

#include "helper.h"
#include <string.h>

// ... (other functions)

const char* get_opcode_name(uint32_t insn) {
    // This is a simplified version. You'd need to expand this for all opcodes.
    switch (insn & 0x7F) {
        case 0x33: return "R-type";
        case 0x13: return "I-type";
        case 0x63: return "B-type";
        case 0x6F: return "J-type";
        case 0x37: return "U-type";
        case 0x73: return "ecall";
        default: return "unknown";
    }
}