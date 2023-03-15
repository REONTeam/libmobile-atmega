name := mobile

dir_source := source
dir_build := build

TARGET_ARCH := -mmcu=atmega328p
TARGET_AVRDUDE := -patmega328p -carduino
SERIAL := /dev/ttyACM0

CC := avr-gcc
OBJCOPY := avr-objcopy
OBJDUMP := avr-objdump
AVRDUDE := avrdude

OPTIM := -Os -g -fdata-sections -ffunction-sections -flto -fuse-linker-plugin -fshort-enums
CFLAGS := $(OPTIM) -Wall -Wextra -std=c11 -DF_CPU=16000000L
LDFLAGS := $(OPTIM) -Wl,--gc-sections -Wl,--print-gc-sections

CPPFLAGS := \
	-I $(dir_source)/libmobile \
	-D MOBILE_ENABLE_IMPL_WEAK

rwildcard = $(foreach d,$(wildcard $1/*),$(filter $2,$d) $(call rwildcard,$d,$2))
objects := $(patsubst $(dir_source)/%.c,$(dir_build)/%.o,$(call rwildcard,$(dir_source),%.c))

.SECONDEXPANSION:

.PHONY: all
all: $(name).hex $(name).lst

.PHONY: clean
clean:
	rm -rf $(dir_build) $(name).hex $(name).lst

.PHONY: upload
upload: $(name).hex $(name).lst
	$(AVRDUDE) -v $(TARGET_AVRDUDE) -P$(SERIAL) -Uflash:w:$<:i

.PHONY: screen
screen: upload
	minicom -D $(SERIAL) -b 500000

%.hex: $(dir_build)/%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.lst: $(dir_build)/%.elf
	$(OBJDUMP) -S $< > $@

$(dir_build)/$(name).elf: $(objects) | $$(dir $$@)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(dir_build)/%.o: $(dir_source)/%.c | $$(dir $$@)
	$(COMPILE.c) -MMD -MP $(OUTPUT_OPTION) $<

.PRECIOUS: %/
%/:
	mkdir -p $@

-include $(objects:.o=.d)
