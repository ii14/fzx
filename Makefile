CMAKE = cmake
JOBS = 1
OPTS =
RELEASE_OPTS = -DFZX_BUILD_BENCHMARKS=ON
DEBUG_OPTS = -DFZX_BUILD_TESTS=ON

help:
	@echo "This is for development only."
	@echo
	@echo "make submodules - initialize and update submodules"
	@echo "make all        - build everything, all build variants with gcc and clang"
.PHONY: help

submodules:
	git submodule update --init
.PHONY: submodules

release-gcc:
	$(CMAKE) --preset release-gcc $(OPTS) $(RELEASE_OPTS)
	$(CMAKE) --build --preset release-gcc -j$(JOBS)
.PHONY: release-gcc

release-clang:
	$(CMAKE) --preset release-clang $(OPTS) $(RELEASE_OPTS)
	$(CMAKE) --build --preset release-clang -j$(JOBS)
.PHONY: release-clang

debug-asan-gcc:
	$(CMAKE) --preset debug-asan-gcc $(OPTS) $(DEBUG_OPTS)
	$(CMAKE) --build --preset debug-asan-gcc -j$(JOBS)
.PHONY: debug-asan-gcc

debug-asan-clang:
	$(CMAKE) --preset debug-asan-clang $(OPTS) $(DEBUG_OPTS)
	$(CMAKE) --build --preset debug-asan-clang -j$(JOBS)
.PHONY: debug-asan-clang

debug-tsan-gcc:
	$(CMAKE) --preset debug-tsan-gcc $(OPTS) $(DEBUG_OPTS)
	$(CMAKE) --build --preset debug-tsan-gcc -j$(JOBS)
.PHONY: debug-tsan-gcc

debug-tsan-clang:
	$(CMAKE) --preset debug-tsan-clang $(OPTS) $(DEBUG_OPTS)
	$(CMAKE) --build --preset debug-tsan-clang -j$(JOBS)
.PHONY: debug-tsan-clang

all: release-gcc release-clang debug-asan-gcc debug-asan-clang debug-tsan-gcc debug-tsan-clang
.PHONY: all
