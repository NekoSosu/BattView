CC := clang
AR := ar
TARGET := battview

SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build

TERMGFX_SRCS := $(shell find $(SRC_DIR)/termgfx -type f -name '*.c')
TERMLAYOUT_SRCS := $(shell find $(SRC_DIR)/termlayout -type f -name '*.c')
APP_SRCS := $(shell find $(SRC_DIR)/app $(SRC_DIR)/frontend $(SRC_DIR)/backend -type f \( -name '*.c' -o -name '*.m' \))

TERMGFX_OBJS := $(TERMGFX_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TERMLAYOUT_OBJS := $(TERMLAYOUT_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
APP_OBJS := $(APP_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
APP_OBJS := $(APP_OBJS:$(SRC_DIR)/%.m=$(BUILD_DIR)/%.o)
OBJS := $(TERMGFX_OBJS) $(TERMLAYOUT_OBJS) $(APP_OBJS)
DEPS := $(OBJS:.o=.d)

LIBTERMGFX := $(BUILD_DIR)/libtermgfx.a
LIBTERMLAYOUT := $(BUILD_DIR)/libtermlayout.a

CPPFLAGS := -I$(INCLUDE_DIR)
CFLAGS := -Wall -Wextra -O2 -fobjc-arc -fblocks
LDFLAGS := -framework Foundation -framework IOKit -framework CoreFoundation -lIOReport

.PHONY: all run clean uml

all: $(TARGET)

$(TARGET): $(APP_OBJS) $(LIBTERMLAYOUT) $(LIBTERMGFX)
	$(CC) $(APP_OBJS) $(LIBTERMLAYOUT) $(LIBTERMGFX) $(LDFLAGS) -o $@

$(LIBTERMGFX): $(TERMGFX_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(LIBTERMLAYOUT): $(TERMLAYOUT_OBJS) $(LIBTERMGFX)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(TERMLAYOUT_OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.m
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

uml: docs/uml.md
	@printf 'UML written to %s\n' "$<"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
