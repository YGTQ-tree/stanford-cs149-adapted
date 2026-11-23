# Program 2: 用 SIMD intrinsics 向量化代码（20 分）

> 本题考查 **SIMD（数据级并行）**：一条指令同时处理多个数据，以及用 **mask** 处理分支和循环次数不同的情况。

## 背景：核心运算是什么

`clampedExpSerial` 对每个元素做“整数次幂再截断”：

```
output[i] = min(values[i] ^ exponents[i], 9.999999)     // exponents[i] == 0 时为 1
```

幂是用**重复乘法**算的，指数是几就乘几次：

```c
if (y == 0) output = 1;
else {
    result = x;
    count  = y - 1;
    while (count > 0) { result *= x; count--; }   // 循环次数 = 指数
    if (result > 9.999999f) result = 9.999999f;
    output = result;
}
```

和 SIMD 有关的难点：

- 一个向量里的几个元素，**指数各不相同**，所以 while 循环的次数也不同。SIMD 的所有 lane 只能一起执行同一条指令。指数小的 lane 早算完了，只能被 mask 关掉，等指数最大的那个 lane。被关掉的 lane 就是浪费掉的算力，也就是“向量利用率”下降的原因。
- 有 `if (y == 0)` 和 `if (result > 9.999999)` 两个分支。SIMD 没有真正的分支，做法是**两边都算，然后用 mask 选**。
- N 不一定是向量宽度的倍数，最后一组要用 mask 关掉越界的 lane。

## 运行

```bash
make
./myexp -s 16          # Part A，N=16
./myexp -s 3 -l        # N=3，并打印向量指令日志
make myexp_avx && ./myexp_avx                        # Part B，AVX2
make myexp_avx AVXFLAGS="-mavx2 -mfma -mavx512f"     # Part B，加上 AVX-512（仅 i5-11300H）
```

---

## Part A：CS149 假 intrinsics

改 [main.cpp](main.cpp) 里的 `clampedExpVector` 和 `arraySumVector`。API 在 [CS149intrin.h](CS149intrin.h)，`absVector` 是一个完整的例子，先读懂它。

### 关键函数

| 作用 | 函数 |
|---|---|
| 生成 mask | `_cs149_init_ones(k)`：前 k 个 lane 为 1 |
| mask 运算 | `_cs149_mask_not / _cs149_mask_and / _cs149_mask_or` |
| 数 mask 里有几个 1 | `_cs149_cntbits(mask)` |
| 读写内存 | `_cs149_vload_float / _int`、`_cs149_vstore_float` |
| 设为常数 | `_cs149_vset_float / _int(vec, value, mask)` |
| 算术 | `_cs149_vmult_float`、`_cs149_vsub_int` |
| 比较，结果写进 mask | `_cs149_vgt_float / _int`、`_cs149_veq_int` |
| 按 mask 复制 | `_cs149_vmove_float(dest, src, mask)` |

所有带 mask 的操作只写 mask 为 1 的 lane，其余 lane 保持原值。

### 1. 实现 `clampedExpVector`

伪代码（每次处理 `VECTOR_WIDTH` 个元素）：

```
for (i = 0; i < N; i += W) {
    valid = init_ones(min(W, N - i))           // 尾部：只开前几个 lane
    x = load(values + i, valid)
    y = load(exponents + i, valid)

    result = 1.0                               // 先当作 y == 0 的结果
    isZero = (y == 0)        & valid
    active = not(isZero)     & valid           // 需要做乘法的 lane

    result = x  (on active)
    count  = y - 1  (on active)
    active = (count > 0) & active

    while (cntbits(active) > 0) {              // 只要还有 lane 没算完
        result = result * x   (on active)
        count  = count - 1    (on active)
        active = (count > 0) & active          // 算完的 lane 退出
    }

    tooBig = (result > 9.999999) & valid
    result = 9.999999  (on tooBig)
    store(output + i, result, valid)           // 只写有效的 lane
}
```

调试：

- 先用 `-s 16` 跑通，再用 `-s 3`、`-s 7`、`-s 10001` 测尾部。
- 用 `-l` 看日志，每一行是一条向量指令，`*` 表示这个 lane 被启用。

### 2. 不同 `VECTOR_WIDTH` 下的向量利用率

改 [CS149intrin.h](CS149intrin.h) 第 2 行的 `VECTOR_WIDTH`，再 `make` 并运行 `./myexp -s 10000`。

思考：一组里有 W 个元素，while 循环要跑 `max(指数)` 次。W 越大，这一组里出现一个很大的指数的可能性就越大。其余 lane 要陪它跑多久？

### 3. 加分：`arraySumVector`

目标复杂度 `N/W + log2(W)`，分两步：

1. 用一个向量寄存器作为累加器，`N/W` 次向量加法把所有元素加到 W 个 lane 里。
2. 把这 W 个 lane 加成一个数，只能用 `log2(W)` 步。

第 2 步提示：看 `CS149intrin.h` 里的说明：

```
hadd:       [0 1 2 3]          -> [0+1 0+1 2+3 2+3]
interleave: [0 1 2 3 4 5 6 7]  -> [0 2 4 6 1 3 5 7]
```

`hadd` 之后相邻两个 lane 的和重复出现了一次。`interleave` 能把这些不重复的部分和挪到前半部分。重复 `log2(W)` 次 `hadd + interleave`，第 0 个 lane 就是总和。先在纸上用 W=4 推一遍。

---

## Part B：真实的 AVX2 / AVX-512

改 [main_avx.cpp](main_avx.cpp)。文件开头有一张“假 intrinsic ↔ 真实 intrinsic”的对照表，`absAVX2` 是完整的例子。

真实硬件和 Part A 最大的区别：**AVX2 没有独立的 mask 寄存器**。mask 就是一个普通的 `__m256`，每个 lane 要么全 1（真），要么全 0（假）。比较指令会产生这样的 mask，`blendv` 按 mask 选值。

### 关键函数（AVX2，8 个 float / int）

| 作用 | 函数 |
|---|---|
| 读写 | `_mm256_loadu_ps`、`_mm256_loadu_si256`、`_mm256_storeu_ps` |
| 尾部读写 | `_mm256_maskload_ps / _epi32`、`_mm256_maskstore_ps` |
| 常数 | `_mm256_set1_ps(1.f)`、`_mm256_set1_epi32(1)` |
| 运算 | `_mm256_mul_ps`、`_mm256_sub_epi32`、`_mm256_min_ps` |
| 比较 | `_mm256_cmpgt_epi32`（int）、`_mm256_cmp_ps(a, b, _CMP_GT_OQ)`（float） |
| 按 mask 选 | `_mm256_blendv_ps(a, b, mask)`：mask 为真取 b，否则取 a |
| 类型转换（只改解释，不改位） | `_mm256_castsi256_ps`、`_mm256_castps_si256` |
| 判断是否还有 lane 活跃 | `_mm256_movemask_ps(m) != 0` 或 `!_mm256_testz_si256(m, m)` |

### 1. 实现 `clampedExpAVX2`

思路和 Part A 一样，只是把 mask 运算换成真实指令：

```
for (i = 0; i < N; i += 8) {
    n = min(8, N - i)
    tail = 前 n 个 lane 为 -1 的 int 向量     // 例如和 {0,1,...,7} 比较 n > lane
    x = maskload(values + i, tail)
    y = maskload(exponents + i, tail)

    result = blendv(x, 1.0, y == 0)           // y==0 的 lane 直接是 1
    count  = y - 1
    active = count > 0                         // int 比较，得到 -1 / 0

    while (any(active)) {
        result = blendv(result, result * x, active)
        count  = count - (active ? 1 : 0)      // 提示：active 本身就是 -1，count + active 即可
        active = count > 0
    }

    result = min(result, 9.999999)
    maskstore(output + i, tail, result)        // 不越界写
}
```

注意：`y == 0` 的 lane 的 `count` 是 -1，不会进入循环，所以 `result` 保持 1。

### 2. 加速比为什么不到 8x

- 想想 Part A 的利用率：8 个 lane 里只要有一个指数大，另外 7 个就在空等。
- 串行版本有 `-fno-tree-vectorize`，所以基线确实是标量代码。
- 数组有多大？读写的数据能放进缓存吗？

### 3. 仅 i5-11300H：`clampedExpAVX512`

AVX-512 有真正的 mask 寄存器 `__mmask16`，和 Part A 的模型几乎一一对应：

```
k_tail  = (n == 16) ? 0xFFFF : (1 << n) - 1
x       = _mm512_maskz_loadu_ps(k_tail, values + i)
active  = _mm512_cmpgt_epi32_mask(count, zero)
result  = _mm512_mask_mul_ps(result, active, result, x)   // 只更新 active 的 lane
count   = _mm512_mask_sub_epi32(count, active, count, one)
while (active != 0) ...
_mm512_mask_storeu_ps(output + i, k_tail, result)
```

思考：宽度翻倍后利用率会怎么变（对比 Part A 第 2 问）？275HX 不支持 AVX-512，它只能用 AVX2，原因是 Intel 在混合架构（P 核 + E 核）的消费级芯片上关掉了 AVX-512，因为 E 核不支持。

### 4. 加分：`arraySumAVX2`

```
acc = 0 (8 lanes)
for (i = 0; i + 8 <= N; i += 8) acc = acc + load(values + i)
// 8 -> 4：把高 128 位加到低 128 位
lo = _mm256_castps256_ps128(acc); hi = _mm256_extractf128_ps(acc, 1)
s  = _mm_add_ps(lo, hi)
// 4 -> 2 -> 1：_mm_movehl_ps / _mm_shuffle_ps / _mm_hadd_ps
// 最后加上尾部的标量元素
```

结果和串行版本略有不同，是因为浮点加法不满足结合律：`(a+b)+c` 和 `a+(b+c)` 的舍入可能不一样。
