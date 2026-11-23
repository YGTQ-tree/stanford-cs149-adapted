# Program 2 has two parts:
#   myexp      : CS149 "fake" vector intrinsics (simulated, with a logger)
#   myexp_avx  : the same function written with REAL AVX2 / AVX-512 intrinsics
include ../common/common.mk

CXXFLAGS=-I../common -O2 -std=c++17 -Wall
# Flags for the real-intrinsics binary. -mavx2 -mfma works on both machines.
# Add -mavx512f (only meaningful on the i5-11300H) to enable the AVX-512 path:
#   make myexp_avx AVXFLAGS="-mavx2 -mfma -mavx512f"
AVXFLAGS ?= -mavx2 -mfma

all: myexp myexp_avx

logger.o: logger.cpp logger.h CS149intrin.h CS149intrin.cpp
	$(CXX) $(CXXFLAGS) -c logger.cpp

CS149intrin.o: CS149intrin.cpp CS149intrin.h logger.cpp logger.h
	$(CXX) $(CXXFLAGS) -c CS149intrin.cpp

myexp: CS149intrin.o logger.o main.cpp
	$(CXX) $(CXXFLAGS) logger.o CS149intrin.o main.cpp -o myexp

myexp_avx: main_avx.cpp ../common/CycleTimer.h
	$(CXX) $(CXXFLAGS) -O3 $(AVXFLAGS) -fno-tree-vectorize main_avx.cpp -o myexp_avx

clean:
	rm -f *.o myexp myexp_avx *~

.PHONY: all clean
