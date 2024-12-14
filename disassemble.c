#include "disassemble.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h> 
#include "helper.h"

void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    const char* opcode;
    int rd = get_bits(instruction, 7, 5);
    int rs1 = get_bits(instruction, 15, 5);
    int rs2 = get_bits(instruction, 20, 5);
    int funct3 = get_bits(instruction, 12, 3);
    int funct7 = get_bits(instruction, 25, 7);

    switch (instruction & 0x7F) { // Mask for opcode
        case 0x33: // R-type instructions
            switch (funct7) {
                case 0:
                    switch (funct3) {
                        case 0: opcode = "add"; break;
                        case 1: opcode = "sll"; break;
                        case 2: opcode = "slt"; break;
                        case 3: opcode = "sltu"; break;
                        case 4: opcode = "xor"; break;
                        case 5: opcode = "srl"; break;
                        case 6: opcode = "or"; break;
                        case 7: opcode = "and"; break;
                        default: opcode = "unknown_R"; break;
                    }
                    break;
                case 1: // RV32M extension
                    switch (funct3) {
                        case 0: opcode = "mul"; break;
                        case 1: opcode = "mulh"; break;
                        case 2: opcode = "mulhsu"; break;
                        case 3: opcode = "mulhu"; break;
                        case 4: opcode = "div"; break;
                        case 5: opcode = "divu"; break;
                        case 6: opcode = "rem"; break;
                        case 7: opcode = "remu"; break;
                        default: opcode = "unknown_M"; break;
                    }
                    break;
                case 32: // SUB, SRA
                    switch (funct3) {
                        case 0: opcode = "sub"; break;
                        case 5: opcode = "sra"; break;
                        default: opcode = "unknown_R"; break;
                    }
                    break;
                default: opcode = "unknown_R"; break;
            }
            snprintf(result, buf_size, "%s x%d, x%d, x%d", opcode, rd, rs1, rs2);
            break;
        
        case 0x13: // I-type instructions
            {
                int imm = sign_extend(get_bits(instruction, 20, 12), 12);
                switch (funct3) {
                    case 0: opcode = "addi"; break;
                    case 1: // SLLI
                        opcode = "slli";
                        imm = get_bits(instruction, 20, 5); // Only uses bottom 5 bits
                        break;
                    case 2: opcode = "slti"; break;
                    case 3: opcode = "sltiu"; break;
                    case 4: opcode = "xori"; break;
                    case 5: // SRLI/SRAI
                        if (get_bits(instruction, 30, 1)) {
                            opcode = "srai";
                        } else {
                            opcode = "srli";
                        }
                        imm = get_bits(instruction, 20, 5); // Only uses bottom 5 bits
                        break;
                    case 6: opcode = "ori"; break;
                    case 7: opcode = "andi"; break;
                    default: opcode = "unknown_I"; break;
                }
                snprintf(result, buf_size, "%s x%d, x%d, %d", opcode, rd, rs1, imm);
            }
            break;

        case 0x03: // Load instructions
            {
                int imm = sign_extend(get_bits(instruction, 20, 12), 12);
                switch (funct3) {
                    case 0: opcode = "lb"; break;
                    case 1: opcode = "lh"; break;
                    case 2: opcode = "lw"; break;
                    case 4: opcode = "lbu"; break;
                    case 5: opcode = "lhu"; break;
                    default: opcode = "unknown_L"; break;
                }
                snprintf(result, buf_size, "%s x%d, %d(x%d)", opcode, rd, imm, rs1);
            }
            break;

        case 0x23: // Store instructions
            {
                int offset = sign_extend(
                    (get_bits(instruction, 7, 5) | 
                    (get_bits(instruction, 25, 7) << 5)), 
                    12);
                switch (funct3) {
                    case 0: opcode = "sb"; break;
                    case 1: opcode = "sh"; break;
                    case 2: opcode = "sw"; break;
                    default: opcode = "unknown_S"; break;
                }
                snprintf(result, buf_size, "%s x%d, %d(x%d)", opcode, rs2, offset, rs1);
            }
            break;

        case 0x63: // B-type (branch)
            {
                int offset = sign_extend(
                    (get_bits(instruction, 8, 4) << 1) | 
                    (get_bits(instruction, 25, 6) << 5) | 
                    (get_bits(instruction, 7, 1) << 11) | 
                    (get_bits(instruction, 31, 1) << 12), 
                    13);
                switch (funct3) {
                    case 0: opcode = "beq"; break;
                    case 1: opcode = "bne"; break;
                    case 4: opcode = "blt"; break;
                    case 5: opcode = "bge"; break;
                    case 6: opcode = "bltu"; break;
                    case 7: opcode = "bgeu"; break;
                    default: opcode = "unknown_B"; break;
                }
                snprintf(result, buf_size, "%s x%d, x%d, %08x", opcode, rs1, rs2, addr + offset);
            }
            break;

        case 0x6F: // J-type (jal)
            {
                int offset = sign_extend(
                    (get_bits(instruction, 21, 10) << 1) | 
                    (get_bits(instruction, 20, 1) << 11) | 
                    (get_bits(instruction, 12, 8) << 12) | 
                    (get_bits(instruction, 31, 1) << 20), 
                    21);
                snprintf(result, buf_size, "jal x%d, %08x", rd, addr + offset);
            }
            break;

        case 0x67: // JALR
            {
                int imm = sign_extend(get_bits(instruction, 20, 12), 12);
                snprintf(result, buf_size, "jalr x%d, %d(x%d)", rd, imm, rs1);
            }
            break;

        case 0x37: // LUI
            {
                uint32_t imm = get_bits(instruction, 12, 20) << 12;
                snprintf(result, buf_size, "lui x%d, 0x%x", rd, imm >> 12);
            }
            break;

        case 0x17: // AUIPC
            {
                uint32_t imm = get_bits(instruction, 12, 20) << 12;
                snprintf(result, buf_size, "auipc x%d, 0x%x", rd, imm >> 12);
            }
            break;

        case 0x73: // System Instructions
            if (funct3 == 0) {
                if (instruction == 0x73) { // ECALL
                    opcode = "ecall";
                    snprintf(result, buf_size, "%s", opcode);
                } else {
                    snprintf(result, buf_size, "unknown_Sys");
                }
            } else {
                snprintf(result, buf_size, "unknown_Sys");
            }
            break;

        default:
            snprintf(result, buf_size, "unknown");
            break;
    }

    // Add symbol information if available
    const char* sym = symbols_value_to_sym(symbols, addr);
    if (sym) {
        char temp[buf_size];
        snprintf(temp, buf_size, "%s ; %s", result, sym);
        strncpy(result, temp, buf_size - 1);
        result[buf_size - 1] = '\0';
    }
}