# To assist in cross-compiling
CC      = $(CROSS_COMPILE)gcc
WINDRES = $(CROSS_COMPILE)windres

LIBUSB_BUILD_PATH ?= $(shell pwd)/libusb_build
EEPROM_SUPPORT    ?= y

OBJS    += flashcmd_api.o spi_controller.o spi_nand_flash.o spi_nor_flash.o ch341a_spi.o timer.o main.o
CFLAGS  += -D_FILE_OFFSET_BITS=64
CFLAGS  += -O2 -std=gnu99 -Wall

CFLAGS  += -I$(LIBUSB_BUILD_PATH)/include
LDFLAGS += -Bstatic -L$(LIBUSB_BUILD_PATH)/lib -lusb-1.0 -Bdynamic

ifeq ($(EEPROM_SUPPORT),y)
CFLAGS += -DEEPROM_SUPPORT
OBJS   += ch341a_i2c.o i2c_eeprom.o spi_eeprom.o
OBJS   += bitbang_microwire.o mw_eeprom.o ch341a_gpio.o
endif

ifneq (,$(findstring mingw,$(shell $(CC) -dumpmachine)))
# Target: Windows
CFLAGS += -posix
OBJS   += res.o
endif

ifneq (,$(findstring darwin,$(shell $(CC) -dumpmachine)))
# Target: MacOS
CFLAGS  += -Wno-gnu-designator
LDFLAGS += -lobjc -Wl,-framework,IOKit -Wl,-framework,CoreFoundation -Wl,-framework,Security
endif

all: SNANDer

$(LIBUSB_BUILD_PATH)/.installed:
	mkdir -p $(LIBUSB_BUILD_PATH)
	cd $(LIBUSB_BUILD_PATH); \
	../libusb-1.0.26/configure --host=$(CROSS_COMPILE:-=) --prefix=$(LIBUSB_BUILD_PATH) --disable-udev; \
	make; \
	make install
	touch $@

res.o: snander.rc
	$(WINDRES) -o $@ -i $<

SNANDer: $(LIBUSB_BUILD_PATH)/.installed $(OBJS)
	$(CC) $(CFLAGS) -s -o $@ $(OBJS) $(LDFLAGS)

clean: 
	rm -f *.o SNANDer*
	rm -rf $(LIBUSB_BUILD_PATH)
