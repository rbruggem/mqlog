SRC=src

LOGLIBPATH:=../src

CFLAGS+=-O0 -g -I$(LOGLIBPATH) -pthread
CFLAGS+=-D_XOPEN_SOURCE=500  # needed for `ftw`
LIBS+=-llog -pthread
LDFLAGS+=-L$(LOGLIBPATH)

program := src/test

CFLAGS+=-Wall -Wextra -Werror -Winit-self -std=c99 -pedantic -fPIC
#CFLAGS+=-fstack-protector-strong # available with gcc >= 4.9.x
CFLAGS+=-Wformat -Werror=format-security
LDFLAGS+=-Wl,-O1 -Wl,--discard-all -Wl,-z,relro

sources := $(wildcard $(SRC)/*.c)
source-to-object = $(subst .c,.o,$(filter %.c,$1))
objects = $(call source-to-object,$(sources))
dependencies = $(subst .o,.d,$(objects))

include_dirs := $(SRC)
CFLAGS += $(addprefix -I,$(include_dirs))
vpath %.h $(include_dirs)

MV := mv -f
RM := rm -f
SED := sed

.PHONY: all
all: build

.PHONY: build
build: $(program)

$(program): $(objects)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

.PHONY: clean
clean:
	$(RM) $(objects) $(dependencies) $(program) $(SRC)/*.gcda $(SRC)/*.gcno

.PHONY: run
run: build
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LOGLIBPATH) ./$(program)

.PHONY: gcov
gcov:
	@$(MAKE) CFLAGS+="$(CFLAGS) --coverage" LDFLAGS+="$(LDFLAGS) --coverage" build

.PHONY: valgrind
valgrind: build
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):${PWD}/../src \
    valgrind \
            --tool=memcheck \
            --read-var-info=yes \
            --leak-check=yes \
            --show-reachable=yes \
            --num-callers=20 \
            --track-fds=yes \
            --track-origins=yes \
            $(program)

# Dependencies
ifneq ($(MAKECMDGOALS),clean)
include $(dependencies)
endif

%.d: %.c
	$(CC) $(CFLAGS) $(TARGET_ARCH) -M $< | \
	$(SED) 's,\($(notdir $*)\.o\) *:,$(dir $@)\1 $@: ,' > $@.tmp
	$(MV) $@.tmp $@
