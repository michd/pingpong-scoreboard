# Name: Makefile
#
# A Makefile for the ATtiny84.
#
# Gratefully taken and tweaked from 
# https://gist.github.com/electronut/5763929

DEVICE      = attiny84
CLOCK      = 160000000
PROGRAMMER = -c avrispmkII 

# lfuse 1111 1111 : 0xFF
#       |||| \\\\- CKSEL 3:0 - Select clock source: 8.0+i MHz crystal oscillator
#       ||||                   with slowly rising power, Tables 6-9, 6-10 in
#       ||||                   datasheet
#       ||\\- SUT1:0 Select startup time - Crystal osc,
#       ||           slowly rising power, Table 6-10 in datasheet
#       |\- CKOUT: Click Output Enable:On
#       \- CKDIV8: Divide clock by 8: Off
#
# hfuse 1101 0100 : 0xD4
#       |||| |\\\- BODLEVEL 2:0: Brownout detection level - set to 4.3V
#       |||| |                   See table 20-7 in datasheet
#       |||| \- EESAVE: Preserve EEPROM through chip erase, enabled
#       |||\- WDTON: Watchdog timer always on
#       ||\- SPIEN: Enable Serial Prgram and Data Downloading - enabled
#       ||          Weirdly, set to 0 this means enabled. Don't turn this off
#       ||          if you want to keep being able to ISP program it.
#       |\- DWEN: DebugWIRE Enable
#       \- RSTDISBL: External reset disable - disabled
#                    Weirdly, also disabled when 1.
#
# efuse 1111 1111 : 0xFF
#       |||| |||\ - SELFPRGEN: Self-Programming Enable
#       |||| |||               Also disabled by being at the default 1.
#       \\\\ \\\- [unused]
FUSES = -U lfuse:w:0xff:m -U hfuse:w:0xd4:m -U efuse:w:0xff:m

OBJECTS = main.o MAX72S19.o pingpong.o animation.o tonegen.o

# Tune the lines below only if you know what you are doing:

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
COMPILE = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

# symbolic targets:
all:  main.hex

%.o: %.c
	$(COMPILE) -c $< -o $@

%.S: %.o
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

%.s: %.c
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

# if you use a bootloader, change the command below appropriately:
load: all
	bootloadHID main.hex

clean:
	rm -f main.hex main.elf $(OBJECTS)

# file targets:
main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS) -Wl,-u,vfprintf -lprintf_flt -lm

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size --format=avr --mcu=$(DEVICE) main.elf
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
