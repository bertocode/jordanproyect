top_srcdir = ../../../..
CROSS_STM32W=1
OPT= -Os -ffunction-sections -fdata-sections
include $(top_srcdir)/Makefile.top

#-------------- No changes above this line, please ---------------------------

EXTRA_MRPROPER+= *.a *.o *.hex

LDSCRIPT = -T$(top_srcdir)/stm32/lib/linker/sections-nokeep.ld

GEN_DEPENDS = 0

VERSION=$(shell git log . |head -1|awk '{print $$2}')
CFLAGS+=-DVERSION="\"$(VERSION)\""
CFLAGS += -Wno-aggressive-loop-optimizations

####### Mandatory label:
TARGETS = $(ARCH)simpleApp $(ARCH)bootloader $(ARCH)masterApp

BINARY_BOOTLOADER  = $(ARCH)bootloader.axf
BINARY_APPLICATION = $(ARCH)simpleApp.axf
BINARY_MASTER      = $(ARCH)masterApp.axf
 
#### Mandatory label:
OBJS= \
	$(ARCH)m2c_defs.o \
	$(ARCH)led.o \
	$(ARCH)stm32w108xx_it.o \
# do not delete this line

EXTRA_OBJS+= \
# do not delete this line

#CFLAGS+= -Werror
#CFLAGS+= -DCORTEXM3_STM32W108=1

#DLIBS+= $(DLIBDIR)/stm32w108xx_stdPeriph.a $(DLIBDIR)/simplemac-library.a
#CFLAGS+= $(LIBLSI_CFLAGS) $(GLIB_CFLAGS) -I$(top_srcdir)/libawd
#LDLIBS+= $(GLIB_LIBS) $(top_srcdir)/libawd/$(ARCH)libawd.a
#LDFLAGS+= -lc

.PHONY: sniffer clean_gdb

all: $(TARGETS)

#EXTRA_CLEAN += $(TARGETS:%=%.o) $(ARCH)libaldomo.a config.sql
#EXTRA_MRPROPER += $(ARCH)aldomo-sniffer

clean_gdb:
		
#$(TARGETS): $(OBJS)	
#	$(CC) -o $@.axf $^ $(EXTRA_OBJS) $(LDSCRIPT_APP) $(LDFLAGS) $(LDLIBS)
#	$(SIZE) $@.axf
#	$(OBJCOPY) $@.axf -O ihex $@.hex

$(TARGETS) : % : $(OBJS) %.o 
	$(CC) -o $@.axf $^ $(EXTRA_OBJS) -T$@-mem.ld $(LDFLAGS) $(LDLIBS)
	$(SIZE) $@.axf
#	$(OBJCOPY) $@.axf -O ihex $@.hex
	
#BINARY := $(shell pwd)/$(BINARY)

read : 
	@[ ! -e "$(BINARY)" ] || (echo "$(BINARY) already exists, won't overwrite" && exit 1)
	openocd \
        -f interface/stlink-v2.cfg \
        -f target/stm32w108_stlink.cfg \
        -c "init" \
        -c "reset halt" \
        -c "sleep 100" \
        -c "wait_halt 2" \
        -c "dump_image $(BINARY) 0x8010000 65536" \
        -c "reset halt" \
        -c "shutdown"

program_node program : $(BINARY_BOOTLOADER) $(BINARY_APPLICATION)
	openocd \
        -f interface/stlink-v2.cfg \
        -f target/stm32w108_stlink.cfg \
        -c "init" \
        -c "reset halt" \
        -c "sleep 100" \
        -c "wait_halt 2" \
        -c "flash write_image erase $(BINARY_APPLICATION)" \
        -c "sleep 100" \
        -c "verify_image $(BINARY_APPLICATION)" \
        -c "flash write_image erase $(BINARY_BOOTLOADER)" \
        -c "sleep 100" \
        -c "verify_image $(BINARY_BOOTLOADER)" \
        -c "sleep 100" \
        -c "flash erase_address pad 0x800FC00 1" \
        -c "flash fillw 0x800FC00 0xFF000001 1" \
        -c "reset" \
        -c "shutdown"
        
program_master master : $(BINARY_MASTER)
	openocd \
        -f interface/stlink-v2.cfg \
        -f target/stm32w108_stlink.cfg \
        -c "init" \
        -c "reset halt" \
        -c "sleep 100" \
        -c "wait_halt 2" \
        -c "flash write_image erase $(BINARY_MASTER)" \
        -c "sleep 100" \
        -c "verify_image $(BINARY_MASTER)" \
        -c "sleep 100" \
        -c "reset" \
        -c "shutdown"
        
debug : $(BINARY)
	openocd \
        -f interface/stlink-v2.cfg \
        -f target/stm32w108_stlink.cfg \
        -c "init" \
        -c "reset halt"

gdb_bl: $(BINARY)
	arm-none-eabi-gdb $(BINARY_BOOTLOADER) \
        --eval-command "target remote localhost:3333" \
        --eval-command "load" \
        --eval-command "monitor reset halt"

gdb_app: $(BINARY)
	arm-none-eabi-gdb $(BINARY_APPLICATION) \
        --eval-command "target remote localhost:3333" \
        --eval-command "load" \
        --eval-command "monitor reset halt"
        
gdb_master: $(BINARY)
	arm-none-eabi-gdb $(BINARY_MASTER)\
        --eval-command "target remote localhost:3333" \
        --eval-command "load" \
        --eval-command "monitor reset halt"
        
hex: $(BINARY)
	hexdump -v -s 0x5000 -n 1024 $(BINARY_APPLICATION) > $(ARCH)appImage.hex
	./generate-hex ./$(ARCH)appImage.hex > ./$(ARCH)appImage-ww.hex 
	
su: mrproper all program
	
#-------------- No changes below this line, please ---------------------------
#include $(top_srcdir)/stm32/Makefile.ocd
include $(top_srcdir)/Makefile.bottom

