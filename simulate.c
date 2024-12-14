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

    while (1) {
        uint32_t insn = memory_rd_w(mem, cpu.pc);
        stats.insns++;

        // Decode and execute each instruction
        if ((insn & 0x7F) == 0x33) { // R-type
            int rd = get_bits(insn, 7, 5);
            int rs1 = get_bits(insn, 15, 5);
            int rs2 = get_bits(insn, 20, 5);
            int funct3 = get_bits(insn, 12, 3);
            int funct7 = get_bits(insn, 25, 7);

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

            switch (funct3) {
                case 0: cpu.regs[rd] = cpu.regs[rs1] + imm; break; // addi
                case 2: cpu.regs[rd] = (int32_t)cpu.regs[rs1] < imm; break; // slti
                case 3: cpu.regs[rd] = cpu.regs[rs1] < (uint32_t)imm; break; // sltiu
                case 4: cpu.regs[rd] = cpu.regs[rs1] ^ imm; break; // xori
                case 6: cpu.regs[rd] = cpu.regs[rs1] | imm; break; // ori
                case 7: cpu.regs[rd] = cpu.regs[rs1] & imm; break; // andi
            }
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, x%d, %d                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, rs1, imm, rd, cpu.regs[rd]);

        } else if ((insn & 0x7F) == 0x63) { // B-type (branch)
            int rs1 = get_bits(insn, 15, 5);
            int rs2 = get_bits(insn, 20, 5);
            int funct3 = get_bits(insn, 12, 3);
            int offset = sign_extend(
                (get_bits(insn, 8, 4) << 1) | 
                (get_bits(insn, 25, 6) << 5) | 
                (get_bits(insn, 7, 1) << 11) | 
                (get_bits(insn, 31, 1) << 12), 
                13);

            uint32_t next_pc = cpu.pc + 4; // Predict not taken
            int branch_taken = 0;

            switch (funct3) {
                case 0: if (cpu.regs[rs1] == cpu.regs[rs2]) { cpu.pc += offset - 4; branch_taken = 1; } break; // beq
                case 1: if (cpu.regs[rs1] != cpu.regs[rs2]) { cpu.pc += offset - 4; branch_taken = 1; } break; // bne
                case 4: if ((int32_t)cpu.regs[rs1] < (int32_t)cpu.regs[rs2]) { cpu.pc += offset - 4; branch_taken = 1; } break; // blt
                case 5: if ((int32_t)cpu.regs[rs1] >= (int32_t)cpu.regs[rs2]) { cpu.pc += offset - 4; branch_taken = 1; } break; // bge
                case 6: if (cpu.regs[rs1] < cpu.regs[rs2]) { cpu.pc += offset -                 6; } break; // bltu
                case 7: if (cpu.regs[rs1] >= cpu.regs[rs2]) { cpu.pc += offset - 4; branch_taken = 1; } break; // bgeu
            }

            stats.branches++;
            if (branch_taken) {
                stats.taken_branches++;
                if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, x%d, %08x            {T} \n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rs1, rs2, cpu.pc + offset);
            } else {
                cpu.pc = next_pc - 4; // Correct PC for next instruction
                if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, x%d, %08x            {N} \n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rs1, rs2, cpu.pc + offset);
            }

        } else if ((insn & 0x7F) == 0x6F) { // J-type (jal)
            int rd = get_bits(insn, 7, 5);
            int offset = sign_extend(
                (get_bits(insn, 21, 10) << 1) | 
                (get_bits(insn, 20, 1) << 11) | 
                (get_bits(insn, 12, 8) << 12) | 
                (get_bits(insn, 31, 1) << 20), 
                21);
            
            cpu.regs[rd] = cpu.pc + 4;
            cpu.pc += offset - 4; // -4 because we add 4 at the end of the loop
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, %08x                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, cpu.pc + offset, rd, cpu.regs[rd]);

        } else if ((insn & 0x7F) == 0x37) { // U-type (lui)
            int rd = get_bits(insn, 7, 5);
            int imm = get_bits(insn, 12, 20) << 12;
            cpu.regs[rd] = imm;
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, %08x                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, imm, rd, cpu.regs[rd]);

 } else if ((insn & 0x7F) == 0x73) { // System Instructions
    if (get_bits(insn, 12, 3) == 0) { // ECALL
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

        } else if ((insn & 0x7F) == 0x03) { // Load
            int rd = get_bits(insn, 7, 5);
            int rs1 = get_bits(insn, 15, 5);
            int funct3 = get_bits(insn, 12, 3);
            int imm = sign_extend(get_bits(insn, 20, 12), 12);
            uint32_t addr = cpu.regs[rs1] + imm;

            switch (funct3) {
                case 0: cpu.regs[rd] = (int32_t)(int8_t)memory_rd_b(mem, addr); break; // lb
                case 1: cpu.regs[rd] = (uint32_t)memory_rd_b(mem, addr); break; // lbu
                case 2: cpu.regs[rd] = (int32_t)(int16_t)memory_rd_h(mem, addr); break; // lh
                case 3: cpu.regs[rd] = (uint32_t)memory_rd_h(mem, addr); break; // lhu
                case 4: cpu.regs[rd] = memory_rd_w(mem, addr); break; // lw
            }
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, %d(x%d)                R[%d] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rd, imm, rs1, rd, cpu.regs[rd]);

        } else if ((insn & 0x7F) == 0x23) { // Store
            int rs1 = get_bits(insn, 15, 5);
            int rs2 = get_bits(insn, 20, 5);
            int funct3 = get_bits(insn, 12, 3);
            int offset = sign_extend(
                (get_bits(insn, 7, 5) << 5) | 
                get_bits(insn, 25, 7), 
                12);
            uint32_t addr = cpu.regs[rs1] + offset;

            switch (funct3) {
                case 0: memory_wr_b(mem, addr, cpu.regs[rs2]); break; // sb
                case 1: memory_wr_h(mem, addr, cpu.regs[rs2]); break; // sh
                case 2: memory_wr_w(mem, addr, cpu.regs[rs2]); break; // sw
            }
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     %s x%d, %d(x%d)                M[%x] <- %x\n", stats.insns, cpu.pc, insn, get_opcode_name(insn), rs2, offset, rs1, addr, cpu.regs[rs2]);

        } else if ((insn & 0x7F) == 0x67) { // jalr
            int rd = get_bits(insn, 7, 5);
            int rs1 = get_bits(insn, 15, 5);
            int imm = sign_extend(get_bits(insn, 20, 12), 12);
            uint32_t next_pc = cpu.regs[rs1] + imm;
            cpu.regs[rd] = cpu.pc + 4; // Link register
            cpu.pc = (next_pc & ~1) - 4; // -4 because we'll add 4 at the end of loop, mask off LSB for alignment
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     jalr x%d, %d(x%d)            R[%d] <- %x, PC <- %x\n", stats.insns, cpu.pc, insn, rd, imm, rs1, rd, cpu.regs[rd], cpu.pc + 4);

        } else if ((insn & 0x7F) == 0x17) { // auipc
            int rd = get_bits(insn, 7, 5);
            int imm = get_bits(insn, 12, 20) << 12;
            cpu.regs[rd] = cpu.pc + imm;
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     auipc x%d, %x                R[%d] <- %x\n", stats.insns, cpu.pc, insn, rd, imm, rd, cpu.regs[rd]);

        } else {
            if (log_file) fprintf(log_file, "%8ld     %08x : %08x     Unhandled instruction\n", stats.insns, cpu.pc, insn);
        }
        
        // Default PC increment
        cpu.pc += 4;
    }

    return stats;
}
