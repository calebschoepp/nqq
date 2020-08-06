# To generate both a release and debug binary just run `make`

SOURCE_DIR := src
BASE_BUILD_DIR := build
RELEASE_BINARY := nqq
DEBUG_BINARY := nqqd
CC         := gcc
CFLAGS     := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
LFLAGS     := -lm

# The following variables are implicitly defined by recursive make calls.
# e.g. @ $(MAKE) nqq MODE=release NAME=nqq
# These variables should not be used in targets not recursivley called.
# When undefined the targets using these variables just aren't generated.
#
# MODE         "debug" or "release".
# NAME         Name of the output executable (and object file directory).
#
# When MODE is defined modify CFLAGS and define BUILD_DIR appropriately
ifeq ($(MODE),debug)
	CFLAGS += -O0 -g $(shell echo $$DEBUG | tr '[:lower:]' '[:upper:]' | tr '-' '_' | sed 's/[^ ]* */-D DEBUG_&/g')
	BUILD_DIR := build/debug
else
	CFLAGS += -O3 -flto
	BUILD_DIR := build/release
endif

# Files
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(BUILD_DIR)/$(NAME)/, $(notdir $(SOURCES:.c=.o)))

# Phony targets ---------------------------------------------------------------

.PHONY: all
all:
	@ $(MAKE) clean --no-print-directory
	@ $(MAKE) release --no-print-directory 
	@ $(MAKE) debug --no-print-directory
	@ $(MAKE) test --no-print-directory

.PHONY: release
release:
	@ printf "Compiling release binary\n"
	@ $(MAKE) build MODE=release NAME=$(RELEASE_BINARY) --no-print-directory

.PHONY: debug
debug:
	@ printf "Compiling debug binary\n"
	@ $(MAKE) build MODE=debug NAME=$(DEBUG_BINARY) --no-print-directory

.PHONY: clean
clean:
	@ rm -rf $(BASE_BUILD_DIR)
	@ rm -f $(RELEASE_BINARY)
	@ rm -f $(DEBUG_BINARY)

.PHONY: test
test:
	@ python3 util/test.py

# Build targets ---------------------------------------------------------------

build: $(NAME)
	@ printf "\n"

# Link the interpreter.
$(NAME): $(OBJECTS)
	@ printf "%3s %s\n" $(CC) $@
	@ mkdir -p build
	@ $(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

# Compile object files.
$(BUILD_DIR)/$(NAME)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%3s %s\n" $(CC) $<
	@ mkdir -p $(BUILD_DIR)/$(NAME)
	@ $(CC) -c $(CFLAGS) -o $@ $<
