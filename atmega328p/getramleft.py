#!/usr/bin/env python3
# Get the amount of RAM left in the program
STACK_SIZE = 0x200  # Same as defined in source/main.c
RAM_SIZE = 0x800  # 2KB

import subprocess

def get_symbol(sym):
    for line in subprocess.check_output(["avr-nm", "build/mobile.elf"]).decode().removesuffix("\n").split("\n"):
        l = line.split(' ', 2)
        if l[2] == sym:
            return int(l[0], 16)
    return 0

address = get_symbol("__bss_end")
if not address:
    exit()

size = address - 0x00800100
left = RAM_SIZE - STACK_SIZE - size
print(left)
