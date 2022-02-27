PROJECT = rdnstun

include mk/flags.mk

LDFLAGS += -pthread

SOURCES := $(sort $(wildcard *.c))
OBJS := $(SOURCES:.c=.o)
EXE := $(PROJECT)

.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	$(RM) $(EXE) $(OBJS) $(PREREQUISITES)

$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

include mk/prerequisties.mk
