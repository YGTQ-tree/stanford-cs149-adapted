# Shared build settings for all programs.
#
# Override any of these on the command line, e.g.
#   make ISPC_TARGET=avx512skx-x16
#   make ISPC=/path/to/ispc

# ISPC compiler: use the one on PATH, otherwise the copy installed by
# scripts/install_ispc.sh.
ISPC_VERSION ?= 1.28.1
ISPC_LOCAL   := $(HOME)/.local/opt/ispc-v$(ISPC_VERSION)-linux/bin/ispc
ISPC ?= $(shell command -v ispc 2>/dev/null || echo $(ISPC_LOCAL))

# ISPC target ISA and gang size.
#   avx2-i32x8     : 8-wide AVX2. Runs on BOTH machines (default, same as the
#                    original CS149 handout).
#   avx512skx-x16  : 16-wide AVX-512. Only on the i5-11300H (Tiger Lake).
#                    The Core Ultra 9 275HX (Arrow Lake) has NO AVX-512.
#   avx2-i32x16    : 16-wide gang executed as two 8-wide AVX2 halves.
ISPC_TARGET ?= avx2-i32x8
ISPC_ARCH   ?= x86-64

CXX ?= g++
CXX := $(CXX) -m64
