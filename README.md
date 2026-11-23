# Assignment 1: 多核 CPU 上的性能分析（Intel 笔记本版）

本作业改编自 Stanford CS149 Assignment 1（https://github.com/stanford-cs149/asst1）。原作业要求在 Stanford 的 myth 机器（4 核 i7-7700K，AVX2）上运行；这里把它改写成可以在你自己的 Intel 笔记本上完成的版本，同时保留原作业的核心学习目标：

1. 单核内的 SIMD 执行（AVX2 / AVX-512 / ISPC）
2. 多核并行（std::thread、ISPC tasks）以及超线程、混合架构的影响
3. 测量、分析并解释并行程序的性能

## 支持的机器

| | Core Ultra 9 275HX（推荐） | i5-11300H |
|---|---|---|
| 核心 / 线程 | 24 核（8 P + 16 E）/ 24 线程，无超线程 | 4 核 / 8 线程（超线程） |
| SIMD | AVX2、FMA、AVX-VNNI，**没有 AVX-512** | AVX2、FMA、**AVX-512** |
| 本作业的默认 ISPC 目标 | `avx2-i32x8` | `avx2-i32x8`（可选 `avx512skx-x16`） |

以下用 `T` 表示 `nproc` 的输出（逻辑核数）。原作业里与“8 线程 = 4 核 × 2 超线程”绑定的问题，都改成了随 `T` 变化的版本。

> 如果你在 WSL2 里运行，Linux 看不到 P/E 核类型和频率，请接通电源、选择最佳性能模式，并在计时时关闭其他负载。

## 环境搭建

```bash
./scripts/machine_info.sh         # 打印 CPU、SIMD 支持、核数，写报告时要用
./scripts/install_ispc.sh         # 安装 ISPC 到 ~/.local/opt（无需 root）
# 需要代理时： https_proxy=http://127.0.0.1:7897 ./scripts/install_ispc.sh
cd prog1_mandelbrot_threads && make && ./mandelbrot -t 2
```

Makefile 会自动找到 `scripts/install_ispc.sh` 装好的 ISPC；也可以 `make ISPC=/path/to/ispc`。ISPC 的目标指令集由 `common/common.mk` 里的 `ISPC_TARGET` 控制，例如 `make ISPC_TARGET=avx512skx-x16`（仅 i5-11300H）。

## 与原作业的主要差异

- **机器与线程数**：不再假设 4 核 8 线程，扫描线程数时用 `T` 以及 `T` 的倍数。
- **Program 2** 增加 Part B：用真实的 AVX2（以及可选的 AVX-512）intrinsics 实现同一个函数。
- **Program 4** 的 AVX2 加分题提供了 `sqrtAVX.cpp` 骨架并接入了 `main.cpp`。
- **Program 6** 的 `data.dat` 来自 Stanford AFS，这里用 `make data` 在本地生成同规模数据（M=1e6, N=100, K=3, 约 800 MB）。
- 修复了原代码在新版 g++ 下缺少头文件的问题；`MAX_THREADS` 从 32 提高到 64。
- 所有 ARM / Mac 相关内容已删除。

---

## Program 1: 用线程并行生成分形（20 分）

构建并运行 `prog1_mandelbrot_threads/`（`make && ./mandelbrot`）。程序生成 Mandelbrot 集合的图像 `mandelbrot-serial.ppm`，每个像素的亮度与判定该点是否属于集合所需的计算量成正比。用 `--view 2` 得到第二个视图。

你的任务是用 `std::thread` 并行化。起始代码在 `mandelbrotThread.cpp` 的 `workerThreadStart()` 里什么都没做。

**要做的事：**

1. 用两个线程并行：线程 0 算上半幅图，线程 1 算下半幅（空间分解）。
2. 扩展到 2, 3, 4, … 个线程，每个线程算一块连续的行。对 **VIEW 1** 画出“相对串行版本的加速比 vs 线程数”的图，线程数取 `{2, 3, 4, 6, 8}` 以及 `T`（i5 上 `T=8`，275HX 上 `T=24`）。加速比是线性的吗？假设原因（提示：看 3 线程那个点；也可以画 VIEW 2）。
3. 在 `workerThreadStart()` 首尾插入计时代码，测量每个线程的耗时，用数据解释上面的图。
4. 改进任务到线程的映射，使加速比在**两个视图**上都接近线性（不允许线程间同步，也不允许对每种线程数硬编码）。提示：有一种很简单的静态分配方式。报告你的方法和 `T` 个线程时的加速比。
5. 把线程数增加到 `2T`。性能有明显提升吗？为什么？
6. （新增）在 275HX 上，`T=24` 个线程里既有 P 核也有 E 核，在 i5 上则有超线程。结合你的计时数据，讨论这两种“非对等”的执行资源对加速比上限的影响。

## Program 2: 用 SIMD intrinsics 向量化代码（20 分）

看 `prog2_vecintrin/main.cpp` 里的 `clampedExpSerial`：对数组每个元素计算 `values[i]` 的 `exponents[i]` 次幂，并把结果截断到 9.999999。本程序分两部分：

- **Part A（必做）**：使用 CS149 的“假”向量 intrinsics（`CS149intrin.h`）。这些函数不会映射到真实 CPU 指令，而是由库模拟，并统计向量指令数和向量利用率，方便调试。
- **Part B（真实硬件）**：在 `main_avx.cpp` 里，用真实的 AVX2（以及在 i5-11300H 上可选的 AVX-512）intrinsics 实现同一个函数，并实测加速比。

### Part A: CS149 假 intrinsics

提示：

- 每条向量指令都可以带一个 mask，mask 中为 0 的 lane 不会被写入。你需要多个 mask 寄存器以及各种 mask 运算。
- 可以用 `_cs149_cntbits` 统计 mask 中 1 的个数。
- 考虑 N 不是 `VECTOR_WIDTH` 的倍数时会发生什么，用 `./myexp -s 3` 测试，`_cs149_init_ones` 会有用。
- `./myexp -l` 会打印向量指令日志；`addUserLog()` 可以加自定义日志。

程序会报告结果是否正确，以及“Total Vector Instructions”（视为性能，假设每条假向量指令占一个周期）和“Vector Utilization”（被启用的 lane 的百分比）。

**要做的事：**

1. 在 `clampedExpVector` 中实现 `clampedExpSerial` 的向量版本，要求对任意 `N` 和 `VECTOR_WIDTH` 都正确。
2. 运行 `./myexp -s 10000`，把 `CS149intrin.h` 中的 `VECTOR_WIDTH` 分别设为 2、4、8、16，记录向量利用率。利用率随宽度增大是上升、下降还是不变？为什么？
3. *加分（1 分）*：实现 `arraySumVector`（可假设 `VECTOR_WIDTH` 整除 `N`），目标运行时间 `N/VECTOR_WIDTH + log2(VECTOR_WIDTH)`，可用 `hadd` 和 `interleave`。

### Part B: 真实的 AVX2 / AVX-512（新增，10 分，含在本题 20 分之内）

```bash
cd prog2_vecintrin
make myexp_avx                                           # AVX2，两台机器都能跑
make myexp_avx AVXFLAGS="-mavx2 -mfma -mavx512f"         # 加上 AVX-512，仅 i5-11300H
./myexp_avx
```

`main_avx.cpp` 里有一个 `absAVX2` 示例，以及一张“CS149 假 intrinsic ↔ 真实 AVX2 intrinsic”的对照表。编译时加了 `-fno-tree-vectorize`，保证串行基线确实是标量代码。

**要做的事：**

1. 实现 `clampedExpAVX2`：8 个 lane，对任意 N 都正确，且不能越界写。尽量用 `_mm256_maskload_ps` / `_mm256_maskstore_ps` 处理尾部，与你 Part A 的思路对应。
2. 报告相对串行的加速比。它接近 8x 吗？为什么不是？（提示：不同 lane 的指数不同，想想 Part A 的利用率，以及内存带宽。）
3. *仅 i5-11300H*：实现 `clampedExpAVX512`，用真正的 mask 寄存器（`__mmask16`）。它比 AVX2 快多少？在 275HX 上该如何解释“没有 AVX-512”这件事？
4. *加分（1 分）*：用 AVX2 实现 `arraySumAVX2`，先在寄存器里累加，最后再做 8 个 lane 的归约。解释它的结果为什么可能和串行结果略有不同。

## Program 3: 用 ISPC 并行生成分形（20 分）

和 Program 1 一样计算 Mandelbrot 图像，但这次用 ISPC：它同时利用 SIMD 单元（单核内）和多核（tasks）。你只需要做一个简单的修改（这个错误只影响性能，不影响正确性）。

### Part 1: ISPC 基础（10 分）

ISPC 程序的执行模型和 C 不同：一个 ISPC 函数被调用时，会并行执行 `programCount` 个程序实例（称为一个 gang），实例编号是 `programIndex`。gang 一直运行到结束，才回到调用的 C 代码。请仔细读两遍这一段。

下面两个函数都计算 `c = a + b`：

```ispc
export void sum(uniform int N, uniform float* a, uniform float* b, uniform float* c) {
  // 假设 programCount 整除 N
  for (uniform int i = 0; i < N; i += programCount)
    c[programIndex + i] = a[programIndex + i] + b[programIndex + i];
}

export void sum2(uniform int N, uniform float* a, uniform float* b, uniform float* c) {
  foreach (i = 0 ... N)
    c[i] = a[i] + b[i];
}
```

`sum` 是命令式的（说明了如何把工作映射到实例），`sum2` 是声明式的（只说明有哪些独立的工作）。ISPC 文档见 <https://ispc.github.io/>。

**要做的事：**

1. 编译并运行 `mandelbrot_ispc`。默认目标是 `avx2-i32x8`（8 路 AVX2）。你预期的最大加速比是多少？实测为什么低于它？（提示：看计算的特性、图像中哪些区域对 SIMD 不友好，比较 view 1 和 `--view 2`。）
2. *仅 i5-11300H*：试 `make clean && make ISPC_TARGET=avx512skx-x16`。加速比变化了吗？gang 变宽后，Mandelbrot 的发散（divergence）问题是更严重还是更轻？

### Part 2: ISPC tasks（10 分）

`mandelbrot_ispc_withtasks` 里的 `launch[2]` 启动 2 个 task，每个 task 计算图像的一块，tasks 可以乱序、并行地在不同核上执行。

**要做的事：**

1. 运行 `mandelbrot_ispc --tasks`。view 1 上加速比是多少？比不用 tasks 的版本快多少？
2. 只修改 `mandelbrot_ispc_withtasks()`，把 task 数改成合适的值，使加速比远高于串行版本（原作业在 myth 上要求超过 32 倍）。请先估算你的机器的上限（核数 × SIMD 宽度，再考虑 P/E 核或超线程的差别），然后扫描不同的 task 数，画出“加速比 vs task 数”的图。你是怎么确定 task 数的？为什么这个数最好？

   **注意**：图像高度是 800 行，起始代码用 `rowsPerTask = height / numTasks` 分块。如果 task 数不能整除 800（例如 24 或 48），余下的行没有 task 去算，程序会因输出与串行版本不一致而校验失败。请选择能整除 800 的 task 数，或者在 `mandelbrot_ispc_task` 里处理余数行（例如用 `min(height, yend)` 截断最后一块，并向上取整计算 `rowsPerTask`）。

   参考数据（275HX，AVX2 8 路，仅供你判断自己的结果是否合理）：task 数 2 / 8 / 16 / 32 / 160 时，加速比大致为 6x / 12x / 19x / 36x / 51x。task 数过多后收益会回落，因为每个 task 太小，调度开销开始显现。
3. *加分（2 分）*：线程抽象（Program 1）和 ISPC task 抽象有什么区别？思想实验：启动 10,000 个 ISPC task 会怎样？启动 10,000 个线程呢？

## Program 4: 迭代法 `sqrt`（15 分）

`prog4_sqrt` 用 ISPC 对 2000 万个 0 到 3 之间的随机数求平方根，使用牛顿法求解方程 `1/x^2 - S = 0`，初始猜测为 1.0。收敛速度取决于初始猜测的精度：输入越接近 1 收敛越快，越接近 0 或 3 越慢（本实现对 (0, 3) 之外的输入不收敛）。见 `handout-images/sqrt_graph.jpg`。

这一题是对 Program 2 和 3 的复习。

**要做的事：**

1. 构建并运行 `sqrt`。报告单核 ISPC（不用 tasks）和用全部核（用 tasks）的加速比。其中多少来自 SIMD，多少来自多核？
2. 修改 `main.cpp` 中 `values` 的赋值，构造一个使 ISPC **加速比最大**的输入，报告 with / without tasks 两个加速比。SIMD 加速改善了吗？多核加速改善了吗？为什么？
3. 构造一个使 ISPC（不用 tasks）相对串行版本加速比**最小**的输入，描述它、说明为什么这样选，并报告结果。效率为什么下降？（注意目标是 `avx2-i32x8`，即 8 路 SIMD。）
4. *加分（最多 2 分）*：在 `sqrtAVX.cpp` 中用 AVX2 intrinsics 手写 `sqrtAVX2`，速度要接近（或超过）不用 tasks 的 ISPC 版本。提示：用 mask 跟踪尚未收敛的 lane，只要有任何 lane 活跃就继续循环；注意 FMA 会改变舍入，各元素的迭代次数可能与串行版略有不同。`main.cpp` 已经接好了这个函数，会打印它的加速比。
5. （新增）在你的机器上，task 数目固定为 64 是合适的吗？用 `T` 个逻辑核估算：当 task 数小于、等于、远大于 `T` 时，负载均衡分别会怎样？

## Program 5: BLAS `saxpy`（10 分）

`saxpy` 计算 `result = scale * X + Y`，其中 X、Y、result 是 N 个元素的向量（N = 2000 万），`scale` 是标量。每读写三个元素只做两次浮点运算（一次乘、一次加）。它是个极易并行的计算，访存规律且每个元素代价固定。

**要做的事：**

1. 编译并运行 `saxpy`。程序会报告 ISPC（不用 tasks）和 ISPC（用 tasks）的性能。用 tasks 的加速比是多少？解释这个结果。你觉得还能显著提升吗（例如重写代码达到接近线性的加速比）？回答是或否，并说明理由。
2. *加分（1 分）*：`main.cpp` 里总带宽按 `TOTAL_BYTES = 4 * N * sizeof(float)` 计算。虽然 saxpy 只读 X、读 Y、写 result 三个元素，乘数 4 是对的，为什么？（提示：想想 CPU 缓存是怎么工作的。）
3. *加分（视情况给分）*：显著提升 saxpy 的性能（不是只提高几个百分点）。如果成功，描述你怎么做的，以及在你的机器上理论上最好能到多少。
4. （新增）用 `scripts/machine_info.sh` 的信息和你的内存规格，估算你的机器的内存带宽上限，并把它与你测得的 GB/s 比较。给出 `T` 个线程下的 saxpy 加速比为什么远低于 `T`。

注意：这题的答案不需要想得太复杂。

## Program 6: 让 `K-Means` 更快（15 分）

`prog6_kmeans` 用 K-Means 算法对一百万个数据点聚类。给定 K 个初始中心，算法迭代更新中心直到满足收敛条件。起始代码已经是正确的实现，但不够快。你的任务是找出需要改进的**位置**以及**怎么**改进。这一题训练的关键技能是**定位性能热点**：先问自己时间花在哪里，在源码中插入计时代码做测量，再针对占大头的部分想办法加速。

**要做的事：**

1. 生成数据集：

   ```bash
   cd prog6_kmeans
   make data        # 在本地生成 data.dat（M=1e6, N=100, K=3，约 800 MB，约需几秒）
   ./kmeans         # 报告总运行时间
   ```

   原作业从 Stanford AFS 读取 `data.dat`，这里改为用 `gendata.cpp` 在本地生成同规模数据。若内存或磁盘紧张，可以用 `./gendata -m 200000`，但要注意：加速比要在**同一份数据**上比较，且不同规模的结果不能直接互比。
2. 运行 `pip install -r requirements.txt` 安装绘图包，再运行 `python3 plot.py` 生成 `start.png` 和 `end.png`。聚类应看起来合理（大多数点被分配到最近的中心；个别点因为 100 维到 2 维的 PCA 投影而看起来不是最近的，这是正常的）。
3. 用 `common/CycleTimer.h` 的 `CycleTimer::currentSeconds()` 在源码里计时。时间主要花在哪个函数里？
4. 基于你的测量改进实现。目标是加速比达到约 2.1 倍或更高（即 旧运行时间 / 新运行时间 >= 2.1）。请说明你是怎么想到解法的，最终方案是什么，以及对应的加速比。写作格式应该是一系列步骤：“我测量了 ...，这让我认为 X。所以我尝试了 ...，结果加速/减速了 ...”。

**约束：**

- 只能修改 `kmeansThread.cpp`。不能改 `stoppingConditionMet`，也不能改 `kMeansThread` 的接口，其余随便改（可以给 `WorkerArgs` 加成员、重写函数、分配新数组）。
- **不要改变实现的功能。** 不能直接删掉主 while 循环，也不能改变 `dist` 的语义。如果算法不收敛，或 `plot.py` 画出的图与起始代码的结果差别很大，说明有问题。
- **重要：** 只允许并行化下列函数中的**一个**：`dist`、`computeAssignments`、`computeCentroids`、`computeCost`。并行写法可以参考 Program 1 的 `mandelbrotThread.cpp`。

提示：

- 原作业参考解法只改动了 20 到 25 行。
- 定位热点之后，注意 K、M、N 的相对大小。
- 优先做回报高的改动，想想这个问题有哪几个维度可以并行。
- **这一题的目的是练习性能分析和调试。** 即使没达到目标，只要你的写作展示了好的、有思考的调试过程，也能拿到大部分分数。
- （新增）本机上 2.1 倍不是上限：275HX 有 24 个核，所以你可能得到远高于 2.1 倍的加速比。请同时报告线程数从 1 到 `T` 的扫描结果，并解释加速比为什么不再线性增长。

## 扩展任务（可选）

- 在你的机器上，把 Program 3 的 mandelbrot 和 Program 5 的 saxpy 用 OpenMP（`#pragma omp parallel for simd`）重写，并和 ISPC 版本比较。
- 在 i5-11300H 上，对比 `avx2-i32x8`、`avx2-i32x16` 和 `avx512skx-x16` 三个 ISPC 目标在 Program 3 和 4 上的差异。
- 在 275HX 上，用 `taskset` 把 Program 1 绑定到 P 核或 E 核，观察线程的性能差异（Linux 原生环境下才能看到 P/E 核的编号；WSL2 下不可见）。

## 提交说明

- 写作文档 `writeup.pdf`（或 `writeup.md`），需注明你使用的机器，并附上 `./scripts/machine_info.sh` 的输出。
- 代码：`prog2_vecintrin/main.cpp`、`prog2_vecintrin/main_avx.cpp`、`prog1_mandelbrot_threads/mandelbrotThread.cpp`、`prog3_mandelbrot_ispc/mandelbrot.ispc`、`prog4_sqrt/`（含 `sqrtAVX.cpp`）、`prog6_kmeans/kmeansThread.cpp`，以及任何加分题的额外代码。
- 所有代码必须可以在你的机器上直接编译运行。

## 计时方法与注意事项

- 把笔记本接通电源，选择最佳性能模式；计时时关闭浏览器等其他负载。
- 笔记本会降频：每个程序多跑几次，以最小值或中位数为准。程序本身已取 3 到 5 次中的最小值。
- 275HX 上线程数越多，P 核与 E 核的差异越明显；报告时请写清楚线程数和机器。

## 资料

- ISPC 文档与示例：<https://ispc.github.io/>
- Intel Intrinsics Guide：<https://www.intel.com/content/www/us/en/docs/intrinsics-guide/>
- 原作业：<https://github.com/stanford-cs149/asst1>
- 并行技术速查：同目录上一级的 `parallelism-cheatsheet.md`（如存在）

## 致谢

本作业改编自 Stanford CS149（Parallel Computing）的 Assignment 1，起始代码来自 <https://github.com/stanford-cs149/asst1>。Mandelbrot 串行代码源自 Intel 的示例，其版权声明保留在源文件中。
