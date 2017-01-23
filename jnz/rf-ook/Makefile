BINARY = rf-ook
V=1 #verbose make
OBJS = main.o

OPENCM3_DIR = ../libopencm3
LDSCRIPT = $(OPENCM3_DIR)/stm32l0xx8.ld
SCRIPT_DIR = ..
default: $(BINARY).bin

#LDFLAGS = -fno-rtti
CPPFLAGS = -fno-rtti

V=1 #verbose make
include $(OPENCM3_DIR)/Makefile.include
