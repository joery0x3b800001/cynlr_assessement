# +----------------------------------------------------------------------------------------+
# | Makefile - CynLr Line-Scan Scanner Pipeline (Fully Dynamic)                         |
# +----------------------------------------------------------------------------------------+

CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O3 -ffast-math \
						-pthread -march=native -mtune=native \
						-funroll-loops -fomit-frame-pointer
INCLUDES := -Iinclude
LDFLAGS  := -pthread

BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests

# ────────── Compiler Specific flags ──────────
ifeq ($(findstring clang++,$(CXX)),clang++)
	CXXFLAGS += -Wno-unknown-warning-option
else
	CXXFLAGS += -Wno-interference-size
endif

# ── Platform Detection & Path Logic ───────────────────────────────────────────
ifeq ($(OS),Windows_NT)
    EXE_EXT     := .exe
    LIB_EXT     := .dll
    PIC_FLAG    :=
    TIMEOUT_CMD := 
		PWSH_UTF8   := powershell -Command "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;"
    MKDIR       := $(PWSH_UTF8) "New-Item -ItemType Directory -Force"
    RM          := $(PWSH_UTF8) "Remove-Item -Recurse -Force"
    IMPLIB      := $(BUILD_DIR)/cynlr.lib
    ifeq ($(findstring clang++,$(CXX)),clang++)
        LIB_FLAGS := -shared -DCYNLR_BUILDING_DLL -Xlinker /IMPLIB:$(IMPLIB)
    else
        LIB_FLAGS := -shared -DCYNLR_BUILDING_DLL -Wl,--out-implib,$(IMPLIB)
    endif
    RUN_PREFIX  := $(BUILD_DIR)\\
    LD_SEARCH   := -L$(BUILD_DIR) -lcynlr
else
    EXE_EXT     :=
    PIC_FLAG    := -fPIC
    UNAME_S     := $(shell uname -s)
    TIMEOUT_CMD := $(shell command -v timeout 2> /dev/null || echo "perl -e 'alarm shift; exec @ARGV' 3")
    MKDIR       := mkdir -p
    RM          := rm -rf
    ifeq ($(UNAME_S),Darwin)
        LIB_EXT   := .dylib
        LIB_FLAGS := -dynamiclib
        RPATH     := -Wl,-rpath,@loader_path
    else
        LIB_EXT   := .so
        LIB_FLAGS := -shared
        RPATH     := -Wl,-rpath,'$$ORIGIN'
    endif
    RUN_PREFIX  := $(BUILD_DIR)/
    LD_SEARCH   := -L$(BUILD_DIR) -lcynlr $(RPATH)
endif

# ── Targets ───────────────────────────────────────────────────────────────────
LIB_CYNLR := $(BUILD_DIR)/libcynlr$(LIB_EXT)
SCANNER   := $(BUILD_DIR)/scanner$(EXE_EXT)
TEST_BIN  := $(BUILD_DIR)/run_tests$(EXE_EXT)

# ── Sources ───────────────────────────────────────────────────────────────────
LIB_SRCS  := $(SRC_DIR)/DataGenerationBlock.cpp \
             $(SRC_DIR)/FilterThresholdBlock.cpp \
             $(SRC_DIR)/Pipeline.cpp

# ── Default params ────────────────────────────────────────────────────────────
M   ?= 16
TV  ?= 100
T   ?= 1000000
CSV ?= data/sample.csv

# +----------------------------------------------------------------------------------------+
# | Rules                                                                                  |
# +----------------------------------------------------------------------------------------+

.PHONY: all scanner tests run run_csv run_dump profile clean help

all: scanner tests

# ── 1. The Shared Library ─────────────────────────────────────────────────────
$(LIB_CYNLR): $(LIB_SRCS) | $(BUILD_DIR)
	@echo "[LIB] Creating Dynamic Shared Library: $@"
	$(CXX) $(CXXFLAGS) $(PIC_FLAG) $(INCLUDES) $(LIB_FLAGS) $(LIB_SRCS) -o $@ $(LDFLAGS)

# ── 2. The Scanner Executable (Dynamic Link) ──────────────────────────────────
scanner: $(SCANNER)

$(SCANNER): $(SRC_DIR)/main.cpp $(LIB_CYNLR)
	@echo "[LD] Linking Dynamic Scanner: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LD_SEARCH) $(LDFLAGS)

# ── 3. The Unit Tests (Dynamic Link) ──────────────────────────────────────────
$(TEST_BIN): $(TEST_DIR)/test_pipeline.cpp $(LIB_CYNLR)
	@echo "[LD] Linking Dynamic Test Binary: $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LD_SEARCH) $(LDFLAGS)

tests: $(TEST_BIN)
	@echo ""
	@echo "Running unit tests (Dynamic Mode)..."
	@echo "──────────────────────────────────────────────────"
	$(RUN_PREFIX)run_tests$(EXE_EXT)

# ── Run Profiles ──────────────────────────────────────────────────────────────

run: scanner
	@echo ""
	@echo "Running in RandomGenerator mode (Dynamic)..."
	@echo "──────────────────────────────────────────────────"
	$(RUN_PREFIX)scanner$(EXE_EXT) --m $(M) --tv $(TV) --t $(T)

run_csv: scanner
	@echo ""
	@echo "Running in CSV mode (Dynamic) with $(CSV)..."
	@echo "──────────────────────────────────────────────────"
	$(RUN_PREFIX)scanner$(EXE_EXT) --m $(M) --tv $(TV) --t $(T) --csv $(CSV)

profile: scanner
	@echo ""
	@echo "Profiling run (Dynamic mode, 500 µs period)..."
	@echo "──────────────────────────────────────────────────"
	$(RUN_PREFIX)scanner$(EXE_EXT) --m $(M) --tv $(TV) --t 500000 --csv $(CSV)

run_dump: scanner
	@echo ""
	@echo "Running Pipeline Dump (Dynamic)..."
	@echo "Saving raw and filtered data to: data/out.csv"
	@echo "──────────────────────────────────────────────────"
	@$(MKDIR) data
	$(RUN_PREFIX)scanner$(EXE_EXT) --m $(M) --tv $(TV) --t $(T) --csv $(CSV) --dump data/out.csv

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
	@echo "  make            Build dynamic scanner + tests"
	@echo "  make scanner    Build shared lib and dynamic scanner"
	@echo "  make tests      Build and run dynamic unit tests"
	@echo "  make run        Run Random mode (dynamic)"
	@echo "  make run_csv    Run CSV mode (dynamic)"
	@echo "  make profile    Profiling run (dynamic)"
	@echo "  make clean      Remove build/ directory"
	@echo "  make run_dump   Run and export input/output to CSV"
	@echo ""
