#!/usr/bin/env bash
# Print the hardware facts you need for the write-up.
set -u
export LC_ALL=C

echo "== CPU =="
lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|L1d|L2|L3' | sed 's/^/  /'

echo
echo "== SIMD support =="
for f in sse4_2 avx avx2 fma avx_vnni avx512f avx512bw avx512vl; do
    if grep -qw "$f" /proc/cpuinfo; then echo "  $f: yes"; else echo "  $f: no"; fi
done

echo
echo "== Hybrid cores (P-core / E-core) =="
if [ -r /sys/devices/cpu_core/cpus ]; then
    echo "  P-cores (cpu_core): $(cat /sys/devices/cpu_core/cpus)"
    echo "  E-cores (cpu_atom): $(cat /sys/devices/cpu_atom/cpus 2>/dev/null || echo none)"
else
    echo "  Not exposed (non-hybrid CPU, or running under WSL2/VM)."
fi

echo
echo "== Virtualization =="
if grep -qi microsoft /proc/version; then
    echo "  WSL2 detected: frequency scaling and core types are hidden from Linux."
    echo "  Use Windows Task Manager / HWiNFO to see clocks. Plug in AC power and"
    echo "  select the best-performance power mode before timing."
else
    echo "  Native Linux."
    g=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
    [ -r "$g" ] && echo "  cpufreq governor: $(cat $g)"
fi

echo
echo "== Toolchain =="
echo "  $(g++ --version | head -1)"
ISPC_BIN="$(command -v ispc || echo "$HOME/.local/opt/ispc-v1.28.1-linux/bin/ispc")"
if [ -x "$ISPC_BIN" ]; then echo "  $("$ISPC_BIN" --version)"; else echo "  ispc: not installed (run scripts/install_ispc.sh)"; fi
