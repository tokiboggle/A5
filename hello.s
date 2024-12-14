    .globl _start
_start:
    li a7, 2        # Load system call number for putchar (2)
    li a0, 72       # Load the ASCII value of 'H' (72 in decimal)
    ecall           # Make the system call (putchar)

    # Exit the program
    li a7, 93       # System call for exit
    ecall
