# 操作系统课程设计 - Makefile

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -g -std=c11 -D_GNU_SOURCE
LDFLAGS := -pthread

SRC_DIR := src
BUILD   := build
TEST    := tests

COMMON_SRC := $(SRC_DIR)/common/util.c
SCHED_SRC  := $(SRC_DIR)/scheduling/scheduling.c
MEM_SRC    := $(SRC_DIR)/memory/memory.c
SYNC_SRC   := $(SRC_DIR)/sync/sync.c
FS_SRC     := $(SRC_DIR)/filesystem/fs.c
MAIN_SRC   := $(SRC_DIR)/main.c

ALL_SRC := $(MAIN_SRC) $(COMMON_SRC) $(SCHED_SRC) $(MEM_SRC) $(SYNC_SRC) $(FS_SRC)

TARGET   := $(BUILD)/os_demo
TEST_BINS := $(BUILD)/test_scheduling $(BUILD)/test_memory $(BUILD)/test_sync $(BUILD)/test_fs

.PHONY: all clean tests run

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(ALL_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(ALL_SRC) $(LDFLAGS)

tests: $(TEST_BINS)

$(BUILD)/test_scheduling: $(TEST)/test_scheduling.c $(SCHED_SRC) $(COMMON_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_memory: $(TEST)/test_memory.c $(MEM_SRC) $(COMMON_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_sync: $(TEST)/test_sync.c $(SYNC_SRC) $(COMMON_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/test_fs: $(TEST)/test_fs.c $(FS_SRC) $(COMMON_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD) fs.img
