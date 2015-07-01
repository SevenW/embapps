#TODO: Move -I vendor/lpcopen/inc into rules.mk
LIBDIR = ../../embello/lib
CFLAGS += -DCORE_M0PLUS
CXXFLAGS += -DCORE_M0PLUS -I. -I$(LIBDIR)/vendor/lpcopen/inc
ISPOPTS += -s
LINK = LPC812.ld #LPC810.ld | LPC812.ld | LPC824.ld Defines memory layout
ARCH = lpc8xx

OBJS = rf-remote-control.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		printf.o printf-retarget.o

default: isp

#LIBDIR = ../../embello/lib
SHARED = $(LIBDIR)/sys-none
include $(SHARED)/rules.mk
