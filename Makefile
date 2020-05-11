# TODO write a bunch of comments up here about general build process
#
#
#

SOURCE_DIR := src
BASE_BUILD_DIR := build
CC         := gcc
CFLAGS     := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter

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
	CFLAGS += -O0 -DDEBUG -g
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
	@ printf "Compilng release binary\n"
	@ $(MAKE) build MODE=release NAME=nqq --no-print-directory

.PHONY: debug
debug:
	@ printf "Compilng debug binary\n"
	@ $(MAKE) build MODE=debug NAME=nqqd --no-print-directory

# TODO store binaries in a place where I don't need to manullay reference them
.PHONY: clean
clean:
	@ rm -rf $(BASE_BUILD_DIR)
	@ rm -f nqq
	@ rm -f nqqd

.PHONY: test
test:
	@ python3 util/test.py

.PHONY: test-verbose
test-verbose:
	@ python3 util/test.py --verbose

# Build targets ---------------------------------------------------------------

build: $(NAME)
	@ printf "\n"

# Link the interpreter.
$(NAME): $(OBJECTS)
	@ printf "%3s %-20s %s\n" $(CC) $@ "$(CFLAGS)"
	@ mkdir -p build
	@ $(CC) $(CFLAGS) $^ -o $@

# Compile object files.
$(BUILD_DIR)/$(NAME)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@ printf "%3s %-20s %s\n" $(CC) $< "$(CFLAGS)"
	@ mkdir -p $(BUILD_DIR)/$(NAME)
	@ $(CC) -c $(CFLAGS) -o $@ $<
