CFLAGS += -DCORE_M0PLUS
CXXFLAGS += -DCORE_M0PLUS -I.
ISPOPTS += -s
LINK = LPC824.ld
ARCH = lpc8xx

OBJS = rf-ook.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		uart.o printf.o printf-retarget.o

default: isp

LIBDIR = ../../embello/lib
SHARED = $(LIBDIR)/sys-none
include $(SHARED)/rules.mk
