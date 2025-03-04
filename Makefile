GTEST_COLOR ?= yes
export GTEST_COLOR

CMAKE ?= cmake
CMAKE_ARGS ?= -Wno-dev

NUM_CPU ?= $(shell sysctl -n hw.logicalcpu 2>/dev/null || echo 1)

CODESIGN_ID ?= $(shell security find-identity -v 2>/dev/null | \
	grep 'Apple Development' | head -1 | awk '{print $$2}')

all: release_build

release_cmake:
	mkdir -p build/Release
	cd build/Release && $(CMAKE) $(CMAKE_ARGS) -DCMAKE_BUILD_TYPE=Release ../..

release_build: release_cmake
	cd build/Release && make -j$(NUM_CPU)

debug_cmake:
	mkdir -p build/Debug
	cd build/Debug && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_SANITIZERS=ON \
		-DBUILD_TESTING=ON \
		-DBUILD_DOCUMENTATION=ON \
		../..

debug_build: debug_cmake
	cd build/Debug && make -j$(NUM_CPU)

.PHONY: test
test: debug_build
	cd build/Debug && make test ARGS="-V"

gen: debug_cmake
	cd build/Debug && make gen

.PHONY: examples
examples:
	mkdir -p build/Examples
	cd build/Examples && $(CMAKE) $(CMAKE_ARGS) ../../examples
	cd build/Examples && make -j$(NUM_CPU)

install:
	cd build/Release && make install

clean:
	rm -rf build
	rm -rf html
	rm -f compile_commands.json

clobber: clean
	rm -f src/*.g.cpp

fmt:
	find -type f -name '*.[ch]pp' -not -name '*.g.*' \
		| xargs clang-format --verbose -i

md:
	markdown-toc --maxdepth 2 --bullets=- -i README.md
	md-authors --format classic --append AUTHORS.md
