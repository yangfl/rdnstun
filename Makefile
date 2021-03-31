PROJECT = rdnstun

DEBUG ?= 1
RELEASE ?= 0

CPPFLAGS ?= -fdiagnostics-color=always
ifeq ($(RELEASE), 0)
	CFLAGS ?= -Os -fPIC
else
	CFLAGS ?= -O2 -fPIC
endif
LDFLAGS ?= -pie

CPPFLAGS += -D_FORTIFY_SOURCE=2
CFLAGS += -std=c2x -fstack-protector-strong
LDFLAGS += -Wl,--gc-sections -Wl,-z,relro

CWARN ?= -Wall -Wextra -Wpedantic -Werror=format-security \
	-Wno-cast-function-type -Wno-missing-field-initializers
ifeq ($(DEBUG), 1)
	CWARN += -fanalyzer
endif
CFLAGS += $(CWARN)

ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif
ifeq ($(RELEASE), 0)
	CFLAGS += -DDEBUG
endif

CPPFLAGS += -D_DEFAULT_SOURCE
CFLAGS +=
LDFLAGS +=

# LIBS :=
# LIBS_CPPFLAGS := $(shell pkg-config --cflags-only-I $(LIBS))
# LIBS_CFLAGS := $(shell pkg-config --cflags-only-other $(LIBS))
# LIBS_LDFLAGS := $(shell pkg-config --libs $(LIBS))

CPPFLAGS += $(LIBS_CPPFLAGS)
CFLAGS += $(LIBS_CFLAGS)
LDFLAGS += $(LIBS_LDFLAGS)

SOURCES := rdnstun.c chain.c checksum.c host.c iface.c log.c
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
