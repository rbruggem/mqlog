SRC=src

MQLOGLIBPATH:=../src

CFLAGS+=-O0 -g -I$(MQLOGLIBPATH) -pthread
CFLAGS+=-D_XOPEN_SOURCE=500  # needed for `ftw`
LIBS+=-lmqlog -pthread -lrt
LDFLAGS+=-L$(MQLOGLIBPATH)

program := src/test

CFLAGS+=-Wall -Wextra -Werror -Winit-self -std=c99 -pedantic -fPIC
CFLAGS+=-Wformat -Werror=format-security

ifeq ($(shell expr `$(CC) -v 2> /dev/stdout  | grep -i gcc | tail -n 1 | sed -e 's/.*\(gcc\).*/\1/I'`),gcc)
GCC_GTEQ_490 := $(shell expr `gcc -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/'` \>= 40900)
ifeq ($(GCC_GTEQ_490),1)
CFLAGS+=-fstack-protector-strong # available with gcc >= 4.9.x
endif
endif

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
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(MQLOGLIBPATH) ./$(program)

.PHONY: valgrind
valgrind: build
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):${MQLOGLIBPATH} \
    valgrind \
            --tool=memcheck \
            --read-var-info=yes \
            --leak-check=yes \
            --show-reachable=yes \
            --num-callers=20 \
            --track-fds=yes \
            --track-origins=yes \
            $(program) -e big_test

# Dependencies
ifneq ($(MAKECMDGOALS),clean)
include $(dependencies)
endif

%.d: %.c
	$(CC) $(CFLAGS) $(TARGET_ARCH) -M $< | \
	$(SED) 's,\($(notdir $*)\.o\) *:,$(dir $@)\1 $@: ,' > $@.tmp
	$(MV) $@.tmp $@
