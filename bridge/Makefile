name := $(notdir $(PWD))

dir_source := source
dir_build := build

CFLAGS := -Og -g -Wall -Wextra -std=gnu17 $(CFLAGS)

CFLAGS += $(shell pkg-config --cflags libserialport)
LDLIBS += $(shell pkg-config --libs libserialport)

SANIT := -fsanitize=address -fsanitize=leak -fsanitize=undefined
OPTIM := -Os -fdata-sections -ffunction-sections -flto -fuse-linker-plugin -Wl,--gc-sections

rwildcard = $(foreach d,$(wildcard $1/*),$(filter $2,$d) $(call rwildcard,$d,$2))
objects := $(patsubst $(dir_source)/%.c,$(dir_build)/%.o,$(call rwildcard,$(dir_source),%.c))

.SECONDEXPANSION:

.PHONY: all
all: $(name)

.PHONY: clean
clean:
	rm -rf $(dir_build) $(name)

.PHONY: sanit
sanit: CFLAGS += $(SANIT)
sanit: LDFLAGS += $(SANIT)
sanit: $(name)

.PHONY: optim
optim: CFLAGS += $(OPTIM)
optim: LDFLAGS += $(OPTIM)
optim: $(name)
	strip --strip-all --strip-unneeded $(name)

$(name): $(objects)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(dir_build)/%.o: $(dir_source)/%.c | $$(dir $$@)
	$(COMPILE.c) -MMD -MP $(OUTPUT_OPTION) $<

.PRECIOUS: %/
%/:
	mkdir -p $@

-include $(objects:.o=.d)
