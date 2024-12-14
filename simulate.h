#ifndef __SIMULATE_H__
#define __SIMULATE_H__

#include "memory.h"
#include "read_elf.h"
#include <stdio.h>

// Simuler RISC-V program i givet lager og fra given start adresse
struct Stat {
    long int insns;         // Number of instructions executed
    long int branches;      // Number of branch instructions encountered
    long int taken_branches; // Number of branches that were taken
};

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols);

#endif