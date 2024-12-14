# Compiler configuration with optimization
GCC=gcc -g -Wall -Wextra -pedantic -std=gnu11 -O

# Default target
all: sim

# Rebuild target
rebuild: clean all

# sim target explicitly lists all source files to ensure they're included
sim: main.c memory.c read_elf.c simulate.c disassemble.c helper.c
	$(GCC) $^ -o sim 

# Zip target for packaging source files
zip: ../src.zip

../src.zip: clean
	cd .. && zip -r src.zip src/Makefile src/*.c src/*.h

# Clean target to remove compiled files
clean:
	rm -rf *.o sim vgcore*