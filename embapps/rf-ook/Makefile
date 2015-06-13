#CFLAGS += -DSTARTUP_NO_IRQS
CXXFLAGS += -I.
ISPOPTS += -t
LINK = LPC824.ld
ARCH = lpc8xx

OBJS = rf_ook.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		uart.o printf.o printf-retarget.o

default: isp

LIBDIR = ../../embello/lib
SHARED = $(LIBDIR)/sys-none
include $(SHARED)/rules.mk
