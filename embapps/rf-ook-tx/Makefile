CFLAGS += -DCORE_M0PLUS
CXXFLAGS += -DCORE_M0PLUS -I.
ISPOPTS += -s
LINK = LPC810.ld #LPC810.ld | LPC812.ld | LPC824.ld Defines memory layout
ARCH = lpc8xx

OBJS = rf-ook-tx.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		uart.o printf.o printf-retarget.o

default: isp

LIBDIR = C:\Github\embello\lib
SHARED = $(LIBDIR)/sys-none
include $(SHARED)/rules.mk
