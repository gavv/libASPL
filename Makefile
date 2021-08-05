GTEST_COLOR ?= yes
export GTEST_COLOR

CMAKE ?= cmake

CMAKE_ARGS ?= -Wno-dev

NUM_CPU ?= `sysctl -n hw.logicalcpu 2>/dev/null || echo 1`

CODESIGN_ID ?= \
  `security find-identity -v 2>/dev/null | grep 'Apple Development' | head -1 | awk '{print $$2}'`

all: release_build

release_build:
	mkdir -p build/Release
	cd build/Release && $(CMAKE) $(CMAKE_ARGS) -DCMAKE_BUILD_TYPE=Release ../..
	cd build/Release && make -j$(NUM_CPU)

debug_build:
	mkdir -p build/Debug
	cd build/Debug && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_SANITIZERS=ON \
		-DBUILD_TESTING=ON \
		-DBUILD_DOCUMENTATION=ON \
		../..
	cd build/Debug && make -j$(NUM_CPU)

.PHONY: test
test: debug_build
	cd build/Debug && make test ARGS="-V"

install:
	cd build/Release && make install

.PHONY: example
example:
	mkdir -p build/Example
	cd build/Example && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Release -DCODESIGN_ID="$(CODESIGN_ID)" ../../example
	cd build/Example && make -j$(NUM_CPU)

clean:
	rm -rf build
	rm -rf html

clobber: clean
	rm -f src/*.g.cpp

fmt:
	find -type f -name '*.[ch]pp' -not -name '*.g.*' \
		| xargs clang-format --verbose -i
