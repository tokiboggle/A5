#include "simulate.h"
#include "memory.h"
#include "read_elf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "helper.h"

// Basic CPU state
struct cpu_state {
    uint32_t regs[32]; // x0 to x31 registers
    uint32_t pc;       // Program Counter
};

static struct cpu_state cpu;

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* __attribute__((unused)) symbols) {
    struct Stat stats = {0};
    cpu.pc = start_addr;
    cpu.regs[0] = 0; // x0 is hardwired to 0

    printf("Simulation started at address 0x%x\n", start_addr);

    while (1) {
        // Fetch the instruction
        uint32_t insn = memory_rd_w(mem, cpu.pc);
        printf("Fetched instruction 0x%x at PC = 0x%x\n", insn, cpu.pc);
        stats.insns++;

        // Decode and execute each instruction
        if ((insn & 0x7F) == 0x33) { // R-type
            int rd = get_bits(insn, 7, 5);
            int rs1 = get_bits(insn, 15, 5);
            int rs2 = get_bits(insn, 20, 5);
            int funct3 = get_bits(insn, 12, 3);
            int funct7 = get_bits(insn, 25, 7);

            printf("R-type instruction: funct7 = %d, funct3 = %d, rd = %d, rs1 = %d, rs2 = %d\n", funct7, funct3, rd, rs1, rs2);

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
                case 1: // MUL
                    switch (funct3) {
                        case 0: cpu.regs[rd] = (int32_t)cpu.regs[rs1] * (int32_t)cpu.regs[rs2]; break; // mul
                        case 1: cpu.regs[rd] = ((int64_t)cpu.regs[rs1] * (int64_t)cpu.regs[rs2]) >> 32; break; // mulh
                        case 2: cpu.regs[rd] = ((int64_t)cpu.regs[rs1] * (int64_t)((int32_t)cpu.regs[rs2])) >> 32; break; // mulhsu
                        case 3: cpu.regs[rd] = ((uint64_t)cpu.regs[rs1] * (uint64_t)cpu.regs[rs2]) >> 32; break; // mulhu
                        case 4: cpu.regs[rd] = (int32_t)cpu.regs[rs1] / (int32_t)cpu.regs[rs2]; break; // div
                        case 5: cpu.regs[rd] = (uint32_t)cpu.regs[rs1] / (uint32_t)cpu.regs[rs2]; break; // divu
                        case 6: cpu.regs[rd] = (int32_t)cpu.regs[rs1] % (int32_t)cpu.regs[rs2]; break; // rem
                        case 7: cpu.regs[rd] = (uint32_t)cpu.regs[rs1] % (uint32_t)cpu.regs[rs2]; break; // remu
                    }
                    break;
                case 32: // SUB, SRA
                    switch (funct3) {
                        case 0: cpu.regs[rd] = cpu.regs[rs1] - cpu.regs[rs2]; break; // sub
                        case 5: cpu.regs[rd] = (int32_t)cpu.regs[rs1] >> (cpu.regs[rs2] & 0x1F); break; // sra
                    }
                    break;
            }
            if (log_file) fprintf(log_file, "%8ld => %08x : %08x     %s x%d, x%d, x%d                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, rs1, rs2, rd, cpu.regs[rd]);

        } else if ((insn & 0x7F) == 0x13) { // I-type
            int rd = get_bits(insn, 7, 5);
            int rs1 = get_bits(insn, 15, 5);
            int imm = sign_extend(get_bits(insn, 20, 12), 12);
            int funct3 = get_bits(insn, 12, 3);

            printf("I-type instruction: funct3 = %d, rd = %d, rs1 = %d, imm = %d\n", funct3, rd, rs1, imm);

            switch (funct3) {
                case 0: cpu.regs[rd] = cpu.regs[rs1] + imm; break; // addi
                case 2: cpu.regs[rd] = (int32_t)cpu.regs[rs1] < imm; break; // slti
                case 3: cpu.regs[rd] = cpu.regs[rs1] < (uint32_t)imm; break; // sltiu
                case 4: cpu.regs[rd] = cpu.regs[rs1] ^ imm; break; // xori
                case 6: cpu.regs[rd] = cpu.regs[rs1] | imm; break; // ori
                case 7: cpu.regs[rd] = cpu.regs[rs1] & imm; break; // andi
            }
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, x%d, %d                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, rs1, imm, rd, cpu.regs[rd]);

        } else if ((insn & 0x7F) == 0x73) { // System Instructions (e.g., ECALL)
            if (get_bits(insn, 12, 3) == 0) { // ECALL
                printf("ECALL detected: a7 = %d, a0 = %d\n", cpu.regs[17], cpu.regs[10]);
                switch (cpu.regs[17]) { // a7 register holds syscall number
                    case 1:  // getchar
                        cpu.regs[10] = getchar(); // a0
                        break;
                    case 2:  // putchar
                        printf("DEBUG: Printing character: %c (0x%x)\n", cpu.regs[10], cpu.regs[10]); // Debug output
                        putchar(cpu.regs[10]); // a0
                        fflush(stdout);  // Ensure immediate output
                        break;
                    case 3:  // exit
                    case 93: // exit_group
                        if (log_file) fprintf(log_file, "Program terminated at %08x\n", cpu.pc);
                        return stats;
                }
                if (log_file) {
                    fprintf(log_file, "%8ld     %08x : %08x     ecall\n", stats.insns, cpu.pc, insn);
                    fprintf(stderr, "ECALL: a7 = %d, a0 = %d\n", cpu.regs[17], cpu.regs[10]); // Debug output
                }
            }

        } else {
            printf("Unhandled instruction: 0x%x\n", insn);
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     Unhandled instruction\n", stats.insns, cpu.pc, insn);
        }

        // Default PC increment
        cpu.pc += 4;
    }

    return stats;
}
