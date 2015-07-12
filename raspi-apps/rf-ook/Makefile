CXXFLAGS += -I../../../embello/lib/arch-raspi -I../../../embello/lib/driver
LDLIBS = -lwiringPi -lwiringPiDev -lpthread

all: rf-ook

clean:
	rm -f *.o rf-ook


# #TODO: Move -I vendor/lpcopen/inc into rules.mk
# LIBDIR = ../../embello/lib
# CFLAGS += -DCORE_M0PLUS
# CXXFLAGS += -DCORE_M0PLUS -I. -I$(LIBDIR)/vendor/lpcopen/inc
# ISPOPTS += -s
# LINK = LPC824.ld
# ARCH = lpc8xx

# OBJS = rf-ook.o system_LPC8xx.o gcc_startup_lpc8xx.o \
		# uart.o printf.o printf-retarget.o

# default: isp

# #LIBDIR = ../../embello/lib
# SHARED = $(LIBDIR)/sys-none
# include $(SHARED)/rules.mk
