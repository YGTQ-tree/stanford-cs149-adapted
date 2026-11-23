# Program 4: 迭代法 `sqrt`（15 分）

> 本题是 Program 2 和 3 的复习：SIMD 效率取决于**同一个 gang 里的 lane 工作量是否一致**，多核效率取决于**task 之间工作量是否一致**。你要通过构造输入数据来验证这两点。

## 背景：核心运算是什么

对 2000 万个 `(0, 3)` 之间的数求平方根，但不直接调用 `sqrt`，而是用**牛顿法**迭代。

想要 `sqrt(S)`，先求 `g = 1/sqrt(S)`，它是方程 `f(g) = 1/g^2 - S = 0` 的根。牛顿迭代 `g ← g - f(g)/f'(g)` 化简后是：

```
g ← (3g - S·g^3) / 2
```

收敛后 `S · g = S / sqrt(S) = sqrt(S)`。代码：

```c
float guess = 1.0f;                                // 固定的初始猜测
float error = fabs(guess * guess * x - 1.f);       // g^2·S 越接近 1，g 越接近 1/sqrt(S)
while (error > 0.00001f) {
    guess = (3.f * guess - x * guess * guess * guess) * 0.5f;
    error = fabs(guess * guess * x - 1.f);
}
output[i] = x * guess;
```

和并行有关的关键性质：**每个元素的迭代次数取决于它的值。**

- 初始猜测是 `1.0`，而 `1/sqrt(1) = 1`，所以 `x` 越接近 1，猜测越准，几次就收敛。
- `x` 越接近 0 或 3，初始误差越大，需要的迭代次数越多。实测：`x=1.0` 是 0 次，`0.5` 和 `1.5` 是 4 次，`0.001` 是 12 次，`2.9` 是 11 次，`2.999` 是 22 次。
- 见 [../handout-images/sqrt_graph.jpg](../handout-images/sqrt_graph.jpg)：横轴是输入值，纵轴是迭代次数。

这和 Program 3 的 Mandelbrot 是同一种情况：工作量不均匀，SIMD 的 lane 要等最慢的那个。

## 运行

```bash
make
./sqrt
```

输出串行、ISPC、ISPC + tasks、AVX2 四个版本的时间和加速比。默认输入是 `(0.001, 2.999)` 上的均匀随机数，在 [main.cpp](main.cpp) 第 38 行：

```cpp
values[i] = .001f + 2.998f * static_cast<float>(rand()) / RAND_MAX;
```

第 2、3 问都是改这一行。

## 要做的事与提示

### 1. 默认输入下的加速比

把总加速比拆成两部分：

```
SIMD 加速比 = 串行时间 / ISPC 时间
多核加速比 = ISPC 时间 / ISPC+tasks 时间
总加速比   = SIMD 加速比 × 多核加速比
```

随机输入下，同一个 gang 里的 8 个数随机分布，几乎总有一个接近 3 的数拖慢整个 gang。

### 2. 构造让 ISPC 加速比**最大**的输入

要让 SIMD 和多核都高效，需要两点：

- **每个 lane 工作量相同**：gang 内没有空等。
- **每个元素工作量足够大**：计算占比高，访存和调度的开销被摊薄。

所以所有元素应该**都相同**，而且都是**需要很多次迭代**的值。想想 sqrt_graph 上哪里最高。

```cpp
values[i] = ...;    // 一个常数
```

问题：这时 SIMD 加速比接近 8x 了吗？多核加速比变了吗？为什么多核加速比变化不大？（提示：随机输入下，64 个 task 之间的总工作量本来就差不多，因为每个 task 都拿到了同样分布的随机数。）

### 3. 构造让 ISPC（不用 tasks）加速比**最小**的输入

目标是让 SIMD **最浪费**：每 8 个元素里，只有 1 个很慢，其余 7 个很快。

```cpp
values[i] = (i % 8 == 0) ? 慢的值 : 快的值;
```

这样每个 gang 都要跑慢元素的迭代次数，但只有 1/8 的 lane 在做有用功。再想一想：

- 快的值选什么？（`1.0` 一次迭代都不需要。）
- 为什么 ISPC 甚至可能**比串行还慢**？SIMD 指令本身、mask 管理、`while` 条件判断都有开销，而串行版本每个快元素几乎不花时间。
- `i % 8` 的 8 来自 gang 宽度。如果用 `avx512skx-x16`，要改成几？

### 4. 加分：手写 AVX2 版 `sqrtAVX2`

改 [sqrtAVX.cpp](sqrtAVX.cpp)。和 Program 2 Part B 的思路一样：

```
for (i = 0; i < N; i += 8) {
    x      = load(values + i)           // 尾部用 maskload，或单独标量处理
    guess  = set1(initialGuess)
    err    = abs(guess*guess*x - 1)
    active = err > threshold            // _mm256_cmp_ps(err, th, _CMP_GT_OQ)

    while (movemask(active) != 0) {
        newGuess = (3*guess - x*guess*guess*guess) * 0.5
        guess    = blendv(guess, newGuess, active)   // 只更新没收敛的 lane
        err      = abs(guess*guess*x - 1)
        active   = err > threshold
    }
    store(output + i, x * guess)
}
```

关键函数：

- 绝对值：`_mm256_andnot_ps(_mm256_set1_ps(-0.f), v)`，把符号位清 0。
- 比较：`_mm256_cmp_ps(a, b, _CMP_GT_OQ)`。
- 是否还有 lane 活跃：`_mm256_movemask_ps(active)`。
- 可以用 `_mm256_fmadd_ps` / `_mm256_fnmadd_ps` 减少指令数，但 FMA 少一次舍入，结果和串行版会在最后几位上不同。`main.cpp` 的校验允许 `1e-4` 的误差。

为什么不用 blendv 也可以？已经收敛的 lane 继续迭代，`guess` 会基本不变，结果仍然正确。但用 blendv 可以保证结果与串行版本更接近。

### 5. task 数固定为 64 合适吗

[sqrt.ispc](sqrt.ispc) 里 `span = N / 64`。用 `T` 表示逻辑核数（i5 是 8，275HX 是 24）：

- task 数 < T：有核闲着。
- task 数 = T：每个核一个 task，只要有一个核慢（E 核、或分到了慢元素），就要等它。
- task 数 ≫ T：负载会被任务系统动态均衡。

64 / 24 不是整数：275HX 上最后一轮只有 16 个 task，8 个核没事做。试试改成 `T` 的倍数（例如 48、96、192）或更大的数，对比一下。注意 `N / span` 的写法在 N 不能整除时会多出一个 task，`sqrt_ispc_task` 用 `min(N, ...)` 处理了越界。
