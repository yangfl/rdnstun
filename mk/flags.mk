DEBUG ?= 1
ifeq ($(DEBUG), 1)
	RELEASE ?= 0
else
	RELEASE ?= 1
endif

# default
CPPFLAGS ?= -fdiagnostics-color=always
ifeq ($(RELEASE), 0)
	CANYFLAGS ?= -Os
else
	CANYFLAGS ?= -Ofast
endif
LDFLAGS ?= -flto

# debug
ifeq ($(DEBUG), 1)
	CANYFLAGS += -g
endif
ifeq ($(RELEASE), 0)
	CANYFLAGS += -DDEBUG
	LDFLAGS += -rdynamic
endif

# hardening
CPPFLAGS += -D_FORTIFY_SOURCE=2
CANYFLAGS += -fstack-protector-strong
LDFLAGS += -fPIE -Wl,--gc-sections -Wl,-z,relro

# diagnosis
CWARN ?= -Wall -Wextra -Wpedantic -Werror=format-security \
	-Wno-cast-function-type -Wno-missing-field-initializers
ifeq ($(DEBUG), 1)
	CWARN += -fanalyzer
endif
CANYFLAGS += $(CWARN)

# lib flags
ifneq ($(strip $(LIBS)),)
  PKG_CONFIG ?= pkg-config
  LIBS_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags-only-I $(LIBS))
  LIBS_CANYFLAGS := $(shell $(PKG_CONFIG) --cflags-only-other $(LIBS))
  LIBS_LDFLAGS := $(shell $(PKG_CONFIG) --libs $(LIBS))

  CPPFLAGS += $(LIBS_CPPFLAGS)
  CANYFLAGS += $(LIBS_CANYFLAGS)
  LDFLAGS += $(LIBS_LDFLAGS)
endif

# program flags
CPPFLAGS += -D_DEFAULT_SOURCE -I.
CANYFLAGS += -fPIC
LDFLAGS += -L.

CFLAGS += $(CANYFLAGS) -std=c2x
CXXFLAGS += $(CANYFLAGS)
