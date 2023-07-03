GCC = g++
CLANG = clang++
CMAKE = cmake

FMT_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_STANDARD=17 -DFMT_TEST=OFF
CATCH2_FLAGS = -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17
BENCHMARK_FLAGS = -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_STANDARD=17 -DBENCHMARK_ENABLE_TESTING=OFF


help:
	@echo "This is for development only."
	@echo
	@echo "make submodules - initialize and update submodules"
	@echo "make deps       - build dependencies, for gcc and clang"
	@echo "make all        - build everything, all build variants with gcc and clang"
.PHONY: help


submodules:
	git submodule update --init
.PHONY: submodules


fmt-gcc:
	@mkdir -p build/deps-gcc/usr
	@mkdir -p build/deps-gcc/fmt
	$(CMAKE) -B build/deps-gcc/fmt -S deps/fmt -DCMAKE_CXX_COMPILER=$(GCC) $(FMT_FLAGS)
	$(CMAKE) --build build/deps-gcc/fmt -j
	$(CMAKE) --install build/deps-gcc/fmt --prefix build/deps-gcc/usr
.PHONY: fmt-gcc

fmt-clang:
	@mkdir -p build/deps-clang/usr
	@mkdir -p build/deps-clang/fmt
	$(CMAKE) -B build/deps-clang/fmt -S deps/fmt -DCMAKE_CXX_COMPILER=$(CLANG) $(FMT_FLAGS)
	$(CMAKE) --build build/deps-clang/fmt -j
	$(CMAKE) --install build/deps-clang/fmt --prefix build/deps-clang/usr
.PHONY: fmt-clang

catch2-gcc:
	@mkdir -p build/deps-gcc/usr
	@mkdir -p build/deps-gcc/Catch2
	$(CMAKE) -B build/deps-gcc/Catch2 -S deps/Catch2 -DCMAKE_CXX_COMPILER=$(GCC) $(CATCH2_FLAGS)
	$(CMAKE) --build build/deps-gcc/Catch2 -j
	$(CMAKE) --install build/deps-gcc/Catch2 --prefix build/deps-gcc/usr
.PHONY: catch2-gcc

catch2-clang:
	@mkdir -p build/deps-clang/usr
	@mkdir -p build/deps-clang/Catch2
	$(CMAKE) -B build/deps-clang/Catch2 -S deps/Catch2 -DCMAKE_CXX_COMPILER=$(CLANG) $(CATCH2_FLAGS)
	$(CMAKE) --build build/deps-clang/Catch2 -j
	$(CMAKE) --install build/deps-clang/Catch2 --prefix build/deps-clang/usr
.PHONY: catch2-clang

benchmark-gcc:
	@mkdir -p build/deps-gcc/usr
	@mkdir -p build/deps-gcc/benchmark
	$(CMAKE) -B build/deps-gcc/benchmark -S deps/benchmark -DCMAKE_CXX_COMPILER=$(GCC) $(BENCHMARK_FLAGS)
	$(CMAKE) --build build/deps-gcc/benchmark -j
	$(CMAKE) --install build/deps-gcc/benchmark --prefix build/deps-gcc/usr
.PHONY: benchmark-gcc

benchmark-clang:
	@mkdir -p build/deps-clang/usr
	@mkdir -p build/deps-clang/benchmark
	$(CMAKE) -B build/deps-clang/benchmark -S deps/benchmark -DCMAKE_CXX_COMPILER=$(CLANG) $(BENCHMARK_FLAGS)
	$(CMAKE) --build build/deps-clang/benchmark -j
	$(CMAKE) --install build/deps-clang/benchmark --prefix build/deps-clang/usr
.PHONY: benchmark-clang

deps-gcc: fmt-gcc catch2-gcc benchmark-gcc
.PHONY: deps-gcc
deps-clang: fmt-clang catch2-clang benchmark-clang
.PHONY: deps-clang
deps: deps-gcc deps-clang
.PHONY: deps


release-gcc:
	cmake --preset release-gcc -DBUILD_BENCHMARKS=ON
	cmake --build --preset release-gcc -j
.PHONY: release-gcc

release-clang:
	cmake --preset release-clang -DBUILD_BENCHMARKS=ON
	cmake --build --preset release-clang -j
.PHONY: release-clang

debug-asan-gcc:
	cmake --preset debug-asan-gcc -DBUILD_TESTS=ON
	cmake --build --preset debug-asan-gcc -j
.PHONY: debug-asan-gcc

debug-asan-clang:
	cmake --preset debug-asan-clang -DBUILD_TESTS=ON
	cmake --build --preset debug-asan-clang -j
.PHONY: debug-asan-clang

debug-tsan-gcc:
	cmake --preset debug-tsan-gcc -DBUILD_TESTS=ON
	cmake --build --preset debug-tsan-gcc -j
.PHONY: debug-tsan-gcc

debug-tsan-clang:
	cmake --preset debug-tsan-clang -DBUILD_TESTS=ON
	cmake --build --preset debug-tsan-clang -j
.PHONY: debug-tsan-clang

all: deps release-gcc release-clang debug-asan-gcc debug-asan-clang debug-tsan-gcc debug-tsan-clang
.PHONY: all
