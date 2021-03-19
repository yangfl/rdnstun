PROJECT = rdnstun

DEBUG = 1

CPPFLAGS ?= -fdiagnostics-color=always
CFLAGS ?= -fPIC
LDFLAGS ?= -fPIE -Wl,--gc-sections

CWARN ?= -Wall -Wpointer-arith -Wuninitialized -Wpedantic
CFLAGS += $(CWARN)

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

SOURCES := rdnstun.c log.c checksum.c
OBJS := $(SOURCES:.c=.o)
PREREQUISITES := $(SOURCES:.c=.d)

EXE := $(PROJECT)


.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	$(RM) $(EXE) $(OBJS) $(PREREQUISITES)

-include $(PREREQUISITES)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< | sed 's,.*\.o *:,$(<:.c=.o) $@: Makefile ,' > $@

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
