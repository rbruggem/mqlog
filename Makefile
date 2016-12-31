SRC=src

CFLAGS+=-O3 -g
LIBS+=
LDFLAGS+=

libraries := src/liblog.so src/liblog.a

CFLAGS+=-Wall -Wextra -Werror -Winit-self -std=c99 -pedantic -fPIC
CFLAGS+=-fstack-protector-strong -Wformat -Werror=format-security
CFLAGS+=-D_BSD_SOURCE # needed for `madvise` and `ftruncate`
LDFLAGS+=-Wl,-O1 -Wl,--discard-all -Wl,-z,relro -shared

sources := $(wildcard $(SRC)/*.c)
source-to-object = $(subst .c,.o,$(filter %.c,$1))
objects = $(call source-to-object,$(sources))
dependencies = $(subst .o,.d,$(objects))

include_dirs := $(SRC)
CFLAGS += $(addprefix -I,$(include_dirs))
vpath %.h $(include_dirs)

MV := mv -f
RM := rm -rf
SED := sed

.PHONY: all
all: build

.PHONY: build
build: $(libraries)

%.so: $(objects)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.a: $(objects)
	$(AR) rcs -o $@ $^

.PHONY: clean
clean:
	$(RM) $(objects) $(dependencies) $(libraries) $(SRC)/*.gcda $(SRC)/*.gcno lcov-html cov.info
	@$(MAKE) -C test clean

.PHONY: test
test: build
	@$(MAKE) -C test run

.PHONY: gcov
gcov:
	@$(MAKE) CFLAGS+="$(CFLAGS) --coverage" LDFLAGS+="$(LDFLAGS) --coverage" build
	@$(MAKE) -C test gcov

.PHONY: lcov
lcov: gcov
	@make -C test run
	@geninfo --no-checksum -o cov.info src
	@genhtml --legend -o lcov-html cov.info

# Dependencies
ifneq ($(MAKECMDGOALS),clean)
include $(dependencies)
endif

%.d: %.c
	$(CC) $(CFLAGS) $(TARGET_ARCH) -M $< | \
	$(SED) 's,\($(notdir $*)\.o\) *:,$(dir $@)\1 $@: ,' > $@.tmp
	$(MV) $@.tmp $@
