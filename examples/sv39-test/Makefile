# Copyright 2025-2026 Nuo Shen, Nanjing University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PREFIX = riscv64-unknown-elf-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
GDB = $(PREFIX)gdb

# Architecture flags
ARCH = rv64gc
ABI = lp64

# Compiler flags
CFLAGS = -march=$(ARCH) -mabi=$(ABI) -nostdlib -nostartfiles \
         -fno-builtin -fno-stack-protector -Wall -Wextra \
         -O0 -g -mcmodel=medany

# Linker flags
LDFLAGS = -T link.ld

# Source files
SRCS = start.S main.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

# Output files
TARGET = test
ELF = $(TARGET).elf
BIN = $(TARGET).bin
DUMP = $(TARGET).dump

# Default target
all: $(BIN) $(DUMP)

# Link ELF file
$(ELF): $(OBJS) link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Built $@"

# Create binary file
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Created binary: $@"

# Create disassembly
$(DUMP): $(ELF)
	$(OBJDUMP) -D $< > $@
	@echo "Created disassembly: $@"

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly files
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(ELF) $(BIN) $(DUMP)

.PHONY: all clean
