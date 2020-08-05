DEBUG = 1

CPPFLAGS ?= -fdiagnostics-color=always
CFLAGS ?= -Wall -fPIC
LDFLAGS ?= -fPIE -Wl,--gc-sections

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
else
	CFLAGS += -Os
	LDFLAGS += -s -flto
endif
CPPFLAGS +=
CFLAGS +=
LDFLAGS +=

# LIBS :=
# LIBS_CPPFLAGS := $(shell pkg-config --cflags-only-I $(LIBS))
# LIBS_CFLAGS := $(shell pkg-config --cflags-only-other $(LIBS))
# LIBS_LDFLAGS := $(shell pkg-config --libs $(LIBS))

CPPFLAGS += $(LIBS_CPPFLAGS)
CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)

SOURCES := rdnstun.c tinyglib.c checksum.c
OBJS := $(SOURCES:.c=.o)
PREREQUISITES := $(SOURCES:.c=.d)

EXE := rdnstun


.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	rm -f $(EXE) $(OBJS) $(PREREQUISITES)

-include $(PREREQUISITES)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< | sed 's,.*\.o *:,$(<:.c=.o) $@: Makefile ,' > $@

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
