GCC = g++
CLANG = clang++
CMAKE = cmake
JOBS = 1
OPTS =

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
	cmake --preset release-gcc -DFZX_BUILD_BENCHMARKS=ON $(OPTS)
	cmake --build --preset release-gcc -j$(JOBS)
.PHONY: release-gcc

release-clang:
	cmake --preset release-clang -DFZX_BUILD_BENCHMARKS=ON $(OPTS)
	cmake --build --preset release-clang -j$(JOBS)
.PHONY: release-clang

debug-asan-gcc:
	cmake --preset debug-asan-gcc -DFZX_BUILD_TESTS=ON $(OPTS)
	cmake --build --preset debug-asan-gcc -j$(JOBS)
.PHONY: debug-asan-gcc

debug-asan-clang:
	cmake --preset debug-asan-clang -DFZX_BUILD_TESTS=ON $(OPTS)
	cmake --build --preset debug-asan-clang -j$(JOBS)
.PHONY: debug-asan-clang

debug-tsan-gcc:
	cmake --preset debug-tsan-gcc -DFZX_BUILD_TESTS=ON $(OPTS)
	cmake --build --preset debug-tsan-gcc -j$(JOBS)
.PHONY: debug-tsan-gcc

debug-tsan-clang:
	cmake --preset debug-tsan-clang -DFZX_BUILD_TESTS=ON $(OPTS)
	cmake --build --preset debug-tsan-clang -j$(JOBS)
.PHONY: debug-tsan-clang

all: release-gcc release-clang debug-asan-gcc debug-asan-clang debug-tsan-gcc debug-tsan-clang
.PHONY: all
