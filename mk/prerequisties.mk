ifeq ($(DEBUG), 1)
PREREQUISITES := $(SOURCES:.c=.d)
THIS_MAKEFILE_LIST := $(MAKEFILE_LIST)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< | sed 's,.*\.o *:,$(<:.c=.o) $@: $(THIS_MAKEFILE_LIST),' > $@

-include $(PREREQUISITES)
else
PREREQUISITES :=
endif
