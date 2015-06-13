#CFLAGS += -DSTARTUP_NO_IRQS
CXXFLAGS += -I.
ISPOPTS += -s
LINK = LPC812.ld #LPC810.ld | LPC812.ld | LPC824.ld Defines memory layout
ARCH = lpc8xx

OBJS = rf-ook-tx.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		uart.o printf.o printf-retarget.o

default: isp

LIBDIR = C:\Github\embello\lib
SHARED = $(LIBDIR)/sys-none
include $(SHARED)/rules.mk
