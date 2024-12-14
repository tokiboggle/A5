#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "helper.h"
#include <stdbool.h>

// Basic CPU state
struct cpu_state {
    uint32_t regs[32]; // x0 to x31 registers
    uint32_t pc;       // Program Counter
};

static struct cpu_state cpu;

// Helper function to extract bits from instruction
static inline uint32_t get_field(uint32_t insn, int start, int len) {
    return (insn >> start) & ((1 << len) - 1);
}

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    (void)symbols;  // Mark parameter as intentionally unused
    struct Stat stats = {0};
    cpu.pc = start_addr;
    cpu.regs[0] = 0; // x0 is hardwired to 0

    printf("Simulation started at address 0x%x\n", start_addr);

    while (1) {
        // Always ensure x0 is 0
        cpu.regs[0] = 0;
        
        // Fetch instruction
        uint32_t insn = memory_rd_w(mem, cpu.pc);
        stats.insns++;

        if (log_file) {
            fprintf(log_file, "%8ld     %08x : %08x     ", stats.insns, cpu.pc, insn);
        }

        // Get opcode (bits 0-6)
        uint32_t opcode = insn & 0x7F;

        switch (opcode) {
            case 0x33: { // R-type
                int rd = get_field(insn, 7, 5);
                int rs1 = get_field(insn, 15, 5);
                int rs2 = get_field(insn, 20, 5);
                int funct3 = get_field(insn, 12, 3);
                int funct7 = get_field(insn, 25, 7);

                switch (funct7) {
                    case 0:
                        switch (funct3) {
                            case 0: cpu.regs[rd] = cpu.regs[rs1] + cpu.regs[rs2]; break; // add
                            case 1: cpu.regs[rd] = cpu.regs[rs1] << (cpu.regs[rs2] & 0x1F); break; // sll
                            case 2: cpu.regs[rd] = (int32_t)cpu.regs[rs1] < (int32_t)cpu.regs[rs2]; break; // slt
                            case 3: cpu.regs[rd] = cpu.regs[rs1] < cpu.regs[rs2]; break; // sltu
                            case 4: cpu.regs[rd] = cpu.regs[rs1] ^ cpu.regs[rs2]; break; // xor
                            case 5: cpu.regs[rd] = cpu.regs[rs1] >> (cpu.regs[rs2] & 0x1F); break; // srl
                            case 6: cpu.regs[rd] = cpu.regs[rs1] | cpu.regs[rs2]; break; // or
                            case 7: cpu.regs[rd] = cpu.regs[rs1] & cpu.regs[rs2]; break; // and
                        }
                        break;
                    case 32:
                        switch (funct3) {
                            case 0: cpu.regs[rd] = cpu.regs[rs1] - cpu.regs[rs2]; break; // sub
                            case 5: cpu.regs[rd] = (int32_t)cpu.regs[rs1] >> (cpu.regs[rs2] & 0x1F); break; // sra
                        }
                        break;
                }
                break;
            }

            case 0x13: { // I-type
                int rd = get_field(insn, 7, 5);
                int rs1 = get_field(insn, 15, 5);
                int imm = sign_extend(get_field(insn, 20, 12), 12);
                int funct3 = get_field(insn, 12, 3);

                switch (funct3) {
                    case 0: cpu.regs[rd] = cpu.regs[rs1] + imm; break; // addi
                    case 2: cpu.regs[rd] = (int32_t)cpu.regs[rs1] < imm; break; // slti
                    case 3: cpu.regs[rd] = cpu.regs[rs1] < (uint32_t)imm; break; // sltiu
                    case 4: cpu.regs[rd] = cpu.regs[rs1] ^ imm; break; // xori
                    case 6: cpu.regs[rd] = cpu.regs[rs1] | imm; break; // ori
                    case 7: cpu.regs[rd] = cpu.regs[rs1] & imm; break; // andi
                }
                break;
            }

            case 0x03: { // Load
                int rd = get_field(insn, 7, 5);
                int rs1 = get_field(insn, 15, 5);
                int imm = sign_extend(get_field(insn, 20, 12), 12);
                int funct3 = get_field(insn, 12, 3);
                uint32_t addr = cpu.regs[rs1] + imm;

                switch (funct3) {
                    case 0: cpu.regs[rd] = sign_extend(memory_rd_b(mem, addr), 8); break; // lb
                    case 1: cpu.regs[rd] = sign_extend(memory_rd_h(mem, addr), 16); break; // lh
                    case 2: cpu.regs[rd] = memory_rd_w(mem, addr); break; // lw
                    case 4: cpu.regs[rd] = memory_rd_b(mem, addr); break; // lbu
                    case 5: cpu.regs[rd] = memory_rd_h(mem, addr); break; // lhu
                }
                break;
            }

            case 0x23: { // Store
                int rs1 = get_field(insn, 15, 5);
                int rs2 = get_field(insn, 20, 5);
                int imm = sign_extend((get_field(insn, 25, 7) << 5) | 
                                     get_field(insn, 7, 5), 12);
                int funct3 = get_field(insn, 12, 3);
                uint32_t addr = cpu.regs[rs1] + imm;

                switch (funct3) {
                    case 0: memory_wr_b(mem, addr, cpu.regs[rs2]); break; // sb
                    case 1: memory_wr_h(mem, addr, cpu.regs[rs2]); break; // sh
                    case 2: memory_wr_w(mem, addr, cpu.regs[rs2]); break; // sw
                }
                break;
            }

            case 0x63: { // Branch
                int rs1 = get_field(insn, 15, 5);
                int rs2 = get_field(insn, 20, 5);
                int funct3 = get_field(insn, 12, 3);
                int imm = sign_extend((get_field(insn, 31, 1) << 12) |
                                    (get_field(insn, 7, 1) << 11) |
                                    (get_field(insn, 25, 6) << 5) |
                                    (get_field(insn, 8, 4) << 1), 13);
                bool take_branch = false;

                switch (funct3) {
                    case 0: take_branch = (cpu.regs[rs1] == cpu.regs[rs2]); break; // beq
                    case 1: take_branch = (cpu.regs[rs1] != cpu.regs[rs2]); break; // bne
                    case 4: take_branch = ((int32_t)cpu.regs[rs1] < (int32_t)cpu.regs[rs2]); break; // blt
                    case 5: take_branch = ((int32_t)cpu.regs[rs1] >= (int32_t)cpu.regs[rs2]); break; // bge
                    case 6: take_branch = (cpu.regs[rs1] < cpu.regs[rs2]); break; // bltu
                    case 7: take_branch = (cpu.regs[rs1] >= cpu.regs[rs2]); break; // bgeu
                }

                if (take_branch) {
                    cpu.pc += imm - 4;  // -4 because we add 4 at the end of the loop
                }
                break;
            }

            case 0x6F: { // JAL
                int rd = get_field(insn, 7, 5);
                int imm = sign_extend((get_field(insn, 31, 1) << 20) |
                                    (get_field(insn, 12, 8) << 12) |
                                    (get_field(insn, 20, 1) << 11) |
                                    (get_field(insn, 21, 10) << 1), 21);
                
                cpu.regs[rd] = cpu.pc + 4;
                cpu.pc += imm - 4;  // -4 because we add 4 at the end of the loop
                break;
            }

            case 0x67: { // JALR
                int rd = get_field(insn, 7, 5);
                int rs1 = get_field(insn, 15, 5);
                int imm = sign_extend(get_field(insn, 20, 12), 12);
                
                uint32_t next_pc = (cpu.regs[rs1] + imm) & ~1;  // Clear least significant bit
                cpu.regs[rd] = cpu.pc + 4;
                cpu.pc = next_pc - 4;  // -4 because we add 4 at the end of the loop
                break;
            }

            case 0x73: { // ECALL
                if (get_field(insn, 12, 3) == 0) {  // ECALL
                    switch (cpu.regs[17]) {  // a7 holds syscall number
                        case 1:  // getchar
                            cpu.regs[10] = getchar();
                            break;
                        case 2:  // putchar
                            putchar(cpu.regs[10]);
                            fflush(stdout);  // Ensure immediate output
                            break;
                        case 3:  // exit
                        case 93: // exit_group
                            if (log_file) {
                                fprintf(log_file, "Program terminated at %08x\n", cpu.pc);
                            }
                            return stats;
                    }
                }
                break;
            }

            case 0x17: { // AUIPC
                int rd = get_field(insn, 7, 5);
                int imm = get_field(insn, 12, 20) << 12;
                cpu.regs[rd] = cpu.pc + imm;
                break;
            }

            case 0x37: { // LUI
                int rd = get_field(insn, 7, 5);
                int imm = get_field(insn, 12, 20) << 12;
                cpu.regs[rd] = imm;
                break;
            }

            default:
                printf("Unhandled instruction at PC = %08x: %08x\n", cpu.pc, insn);
                if (log_file) {
                    fprintf(log_file, "Unhandled instruction\n");
                }
                return stats;
        }

        // Log register updates if relevant
        if (log_file) {
            // Add logging code here based on instruction type
            fprintf(log_file, "\n");
        }

        cpu.pc += 4;  // Move to next instruction
        cpu.regs[0] = 0;  // Ensure x0 stays 0
    }

    return stats;
}