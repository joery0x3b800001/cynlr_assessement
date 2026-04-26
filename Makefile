# ══════════════════════════════════════════════════════════════════════════════
# Makefile - CynLr Line-Scan Scanner Pipeline (Fully Dynamic)
# ══════════════════════════════════════════════════════════════════════════════

CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O3 -ffast-math -pthread
INCLUDES := -Iinclude
LDFLAGS  := -pthread

# ── Platform Detection & Path Logic ───────────────────────────────────────────
ifeq ($(OS),Windows_NT)
    EXE_EXT := .exe
    LIB_EXT := .dll
    LIB_FLAGS := -shared
    TIMEOUT_CMD := 
    MKDIR := powershell -Command "New-Item -ItemType Directory -Force"
    RM    := powershell -Command "Remove-Item -Recurse -Force"
    RPATH := 
else
    EXE_EXT :=
    UNAME_S := $(shell uname -s)
    TIMEOUT_CMD := $(shell command -v timeout 2> /dev/null || echo "perl -e 'alarm shift; exec @ARGV' 3")
    MKDIR := mkdir -p
    RM    := rm -rf
    ifeq ($(UNAME_S),Darwin)
        LIB_EXT := .dylib
        LIB_FLAGS := -dynamiclib
        RPATH := -Wl,-rpath,@loader_path
    else
        LIB_EXT := .so
        LIB_FLAGS := -shared
        RPATH := -Wl,-rpath,'$$ORIGIN'
    endif
endif

BUILD_DIR  := build
SRC_DIR    := src
TEST_DIR   := tests

# ── Targets ───────────────────────────────────────────────────────────────────
LIB_CYNLR     := $(BUILD_DIR)/libcynlr$(LIB_EXT)
SCANNER       := $(BUILD_DIR)/scanner$(EXE_EXT)
TEST_BIN      := $(BUILD_DIR)/run_tests$(EXE_EXT)

# ── Sources ───────────────────────────────────────────────────────────────────
# Core logic to be bundled into the shared library
LIB_SRCS  := $(SRC_DIR)/DataGenerationBlock.cpp \
             $(SRC_DIR)/FilterThresholdBlock.cpp \
             $(SRC_DIR)/Pipeline.cpp

# ── Default params ────────────────────────────────────────────────────────────
M   ?= 16
TV  ?= 100
T   ?= 1000000
CSV ?= data/sample.csv

# ══════════════════════════════════════════════════════════════════════════════
# Rules
# ══════════════════════════════════════════════════════════════════════════════

.PHONY: all scanner tests run run_csv run_dump profile clean help

all: scanner tests

# ── 1. The Shared Library ─────────────────────────────────────────────────────
$(LIB_CYNLR): $(LIB_SRCS) | $(BUILD_DIR)
	@echo "[LIB] Creating Shared Library: $@"
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) $(LIB_FLAGS) $(LIB_SRCS) -o $@ $(LDFLAGS)

# ── 2. The Scanner Executable (Dynamic Link) ──────────────────────────────────
scanner: $(SCANNER)

$(SCANNER): $(SRC_DIR)/main.cpp $(LIB_CYNLR)
	@echo "[LD] Dynamic Scanner: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -L$(BUILD_DIR) -lcynlr $(LDFLAGS) $(RPATH)

# ── 3. The Unit Tests (Dynamic Link) ──────────────────────────────────────────
$(TEST_BIN): $(TEST_DIR)/test_pipeline.cpp $(LIB_CYNLR)
	@echo "[LD] Dynamic Test Binary: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ -L$(BUILD_DIR) -lcynlr $(LDFLAGS) $(RPATH)

tests: $(TEST_BIN)
	@echo ""
	@echo "Running unit tests (Dynamic Mode)..."
	@echo "──────────────────────────────────────────────────"
	./$(TEST_BIN)

# ── Run Profiles (All using Dynamic Scanner) ──────────────────────────────────

run: scanner
	@echo ""
	@echo "Running in RandomGenerator mode (Dynamic)..."
	@echo "──────────────────────────────────────────────────"
	$(TIMEOUT_CMD) ./$(SCANNER) --m $(M) --tv $(TV) --t $(T) || true

run_csv: scanner
	@echo ""
	@echo "Running in CSV mode (Dynamic) with $(CSV)..."
	@echo "──────────────────────────────────────────────────"
	./$(SCANNER) --m $(M) --tv $(TV) --t $(T) --csv $(CSV)

profile: scanner
	@echo ""
	@echo "Profiling run (Dynamic mode, 500 µs period)..."
	@echo "──────────────────────────────────────────────────"
	./$(SCANNER) --m $(M) --tv $(TV) --t 500000 --csv $(CSV)

run_dump: scanner
	@echo ""
	@echo "Running Pipeline Dump (Dynamic)..."
	@echo "Saving raw and filtered data to: data/out.csv"
	@echo "──────────────────────────────────────────────────"
	@mkdir -p data
	./$(SCANNER) --m $(M) --tv $(TV) --t $(T) --csv $(CSV) --dump data/out.csv

# ── Utility ───────────────────────────────────────────────────────────────────

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

clean:
	@echo "[CLEAN] removing $(BUILD_DIR)/"
	$(RM) $(BUILD_DIR)

help:
	@echo ""
	@echo "CynLr Scanner Pipeline - Makefile help"
	@echo "────────────────────────────────────────"
	@echo "  make              Build dynamic scanner + tests"
	@echo "  make scanner      Build shared lib and dynamic scanner"
	@echo "  make tests        Build and run dynamic unit tests"
	@echo "  make run          Run Random mode (dynamic)"
	@echo "  make run_csv      Run CSV mode (dynamic)"
	@echo "  make profile      Profiling run (dynamic)"
	@echo "  make clean        Remove build/ directory"
	@echo "  make run_dump     Run and export input/output to CSV"
	@echo ""
	@echo "Note: All executables link to $(LIB_CYNLR) dynamically."
	@echo ""