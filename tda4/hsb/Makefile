# Fox Red Labs: build NVIDIA's Holoscan Sensor Bridge emulator (Apache-2.0, third-party/holoscan-sensor-bridge)
# on the SK-TDA4VM with the toolchain on TI's Edge AI image (gcc 13, zlib). No CMake, no CUDA: the emulator's only
# CUDA use is a GPU-to-host copy in the Linux data plane, stubbed in stub/cuda_runtime.h. Measured build: 27 s on the board.
#
# On the board: copy this folder and third-party/holoscan-sensor-bridge keeping the relative layout, then `make -j2`.
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wno-missing-field-initializers -Wno-unused-variable -DSEPARATE_FRAMEMETADATA_PACKET=0
TP = ../../third-party/holoscan-sensor-bridge
E = $(TP)/src/hololink/emulation
# NVIDIA's CMake puts the platform directory first; the common sources rely on that order.
INC = -I$(E)/src/linux -I$(E)/src/common -I$(E)/src -I$(E) -I$(TP)/src/hololink -I$(TP)/src -I$(TP) -Istub
COMMON_OBJS = $(patsubst $(E)/src/common/%.cpp,build/common/%.o,$(wildcard $(E)/src/common/*.cpp))
LINUX_OBJS = $(patsubst $(E)/src/linux/%.cpp,build/linux/%.o,$(wildcard $(E)/src/linux/*.cpp))

all: build/libhsbemu.a serve_frames

build/common/%.o: $(E)/src/common/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

build/linux/%.o: $(E)/src/linux/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

build/libhsbemu.a: $(COMMON_OBJS) $(LINUX_OBJS)
	ar rcs $@ $^

serve_frames: serve_frames.cpp build/libhsbemu.a
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@ -Wl,--whole-archive build/libhsbemu.a -Wl,--no-whole-archive -lz -lpthread

clean:
	rm -rf build serve_frames
