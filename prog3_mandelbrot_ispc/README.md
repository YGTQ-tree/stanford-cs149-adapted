# Program 3: 用 ISPC 并行生成分形（20 分）

> 本题考查 **SIMD + 多核**：Part 1 用 ISPC 的 gang 利用一个核里的 SIMD 单元，Part 2 用 ISPC 的 task 把工作分给多个核。

## 背景：核心运算是什么

计算和 Program 1 完全一样：对每个像素反复迭代 `z = z^2 + c`，记录逃逸前的迭代次数（上限 256）。见 [../prog1_mandelbrot_threads/README.md](../prog1_mandelbrot_threads/README.md) 的背景一节。

不同的是**谁来并行**：

- Program 1：你手动创建线程，手动分配行。
- Program 3：用 ISPC 写一个“对单个像素的标量程序”，ISPC 编译器把它变成 SIMD 代码。一次调用会同时运行 `programCount` 个程序实例（一个 gang），每个实例算一个像素，正好填满一条 SIMD 指令的 lane。

在 [mandelbrot.ispc](mandelbrot.ispc) 里，`mandel()` 看起来就是普通的 C 代码，但它里面的 `z_re`、`z_im`、`i` 都是 **varying** 变量：每个 lane 都有自己的一份值。`for` 循环里的 `break` 会让该 lane 退出，其余 lane 继续跑，这和 Program 2 的 mask 机制是一回事，只是 ISPC 替你写好了。

## ISPC 速查

| 概念 | 含义 |
|---|---|
| `uniform` | 整个 gang 共享一个值（标量） |
| varying（默认） | 每个程序实例一个值（向量） |
| `programCount` | gang 的大小。`avx2-i32x8` 下是 8 |
| `programIndex` | 当前实例的编号，0 到 `programCount-1` |
| `foreach (i = a ... b)` | 把 `[a, b)` 分给 gang 里的实例，自动处理尾部 |
| `task void f(...)` | 可以被 `launch` 的函数 |
| `launch[n] f(...)` | 创建 n 个 task，由任务系统分配到各个核上执行 |
| `taskIndex` / `taskCount` | 当前 task 的编号和总数 |

`foreach (j = 0 ... height, i = 0 ... width)` 是二维的，ISPC 会把相邻的 `i` 放进同一个 gang。

## 运行

```bash
make
./mandelbrot_ispc               # Part 1，VIEW 1
./mandelbrot_ispc -v 2          # VIEW 2
./mandelbrot_ispc --tasks       # Part 2
make clean && make ISPC_TARGET=avx512skx-x16   # 仅 i5-11300H：16 路 AVX-512
```

Makefile 加了 `--opt=disable-fma`，保证 ISPC 和串行版本的结果逐位相同，校验才会通过。

---

## Part 1：ISPC 基础

### 1. 预期加速比与实测加速比

- **上限**：`avx2-i32x8` 一条指令处理 8 个 float，所以理想情况下是 8x。
- **为什么到不了 8x**：一个 gang 里的 8 个像素是图像里**相邻的 8 个像素**。gang 要跑到这 8 个里迭代次数最多的那个结束才能退出 `mandel()`。
  - 在纯黑区域（全部 256 次）或纯暗区域（全部很早逃逸），8 个像素的迭代次数相近，lane 几乎没有浪费。
  - 在集合**边界**附近，相邻像素的迭代次数可能差几十上百次，大部分 lane 在空等。
- 比较 view 1 和 view 2：哪个视图的边界区域占比更大？加速比是否对应地更低？

可以用这个估算来验证想法：

```
SIMD 效率 ≈ Σ(每个像素的迭代次数) / Σ(每个 gang 的 max 迭代次数 × 8)
```

（不需要真的写代码算，能讲清楚即可。有兴趣可以在串行版本里统计。）

### 2. 仅 i5-11300H：16 路 AVX-512

gang 从 8 变成 16 后：

- 单条指令的吞吐翻倍。
- 但一个 gang 覆盖了 16 个相邻像素，**落在边界上的可能性更大**，等待最慢 lane 的代价更高。

两者哪个影响更大？和 Program 2 Part A 第 2 问的结论对比。

---

## Part 2：ISPC tasks

### 1. `--tasks` 的加速比

起始代码用 `launch[2]`，只有 2 个 task，最多用到 2 个核。加速比大约是 Part 1 的 2 倍左右吗？不是的话为什么？（提示：和 Program 1 两线程时的问题一样，上下两半的计算量一样吗？）

### 2. 选择 task 数

只改 `mandelbrot_ispc_withtasks()`：

```ispc
uniform int numTasks    = ...;               // 你来选
uniform int rowsPerTask = height / numTasks;
launch[numTasks] mandelbrot_ispc_task(..., rowsPerTask, ...);
```

**必须注意整除问题**：图像有 800 行，如果 `numTasks` 不能整除 800，最后几行没人算，校验会失败（例如 24 个 task 只覆盖 792 行）。两种处理办法：

1. 只选 800 的约数：2, 4, 5, 8, 10, 16, 20, 25, 32, 40, 50, 80, 100, 160, 200, …
2. 让 task 处理余数（推荐，更通用）：

   ```ispc
   // withtasks 里：向上取整
   uniform int rowsPerTask = (height + numTasks - 1) / numTasks;
   // task 里：截断最后一块
   uniform int yend = min(height, ystart + rowsPerTask);
   ```

怎么选 task 数：

- 少于核数：有核闲着。
- 等于核数：每个核一个 task，但每块的计算量不同，最慢的那块决定总时间（Program 1 的问题又出现了）。
- 远多于核数：任务系统会把 task 动态分给空闲的核，先做完的核接着拿下一个 task，负载自然均衡。
- 太多：每个 task 只有几行，创建和调度的开销开始占比。

扫描一组 task 数，画“加速比 vs task 数”图，找到拐点。275HX 上的参考数据（仅供判断结果是否合理）：

| task 数 | 2 | 8 | 16 | 32 | 160 |
|---|---|---|---|---|---|
| 加速比 | ~6x | ~12x | ~19x | ~36x | ~51x |

### 3. 加分：线程和 task 的区别

思想实验：

- 启动 10,000 个 `std::thread`：每个线程都要有自己的栈（默认几 MB）和内核调度实体，操作系统要在它们之间切换。
- 启动 10,000 个 ISPC task：task 只是一个“待做的工作”的描述，放进队列里。真正执行它们的是固定数量的工作线程（见 [../common/tasksys.cpp](../common/tasksys.cpp)，默认按逻辑核数创建线程池）。

问题是：task 和线程，哪个是“要做的工作”，哪个是“执行工作的资源”？
