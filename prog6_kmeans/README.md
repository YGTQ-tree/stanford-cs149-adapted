# Program 6: 让 K-Means 更快（15 分）

> 本题考查**先测量再优化**：给你一段没写过的代码，先用计时找到真正慢的地方，再只针对那一部分做并行化。

## 背景：核心运算是什么

K-Means 把 M 个点分成 K 类。每一类有一个中心点（centroid），每个点属于离它最近的中心。数据规模：

```
M = 1,000,000 个点，N = 100 维，K = 3 类
data[m * N + n]           // 第 m 个点的第 n 维，按“点优先”存放
clusterCentroids[k * N + n]
```

算法反复执行三步，直到每一类的代价都不再明显变化：

```
while (!stoppingConditionMet(prevCost, currCost, epsilon, K)) {
    computeAssignments()   // 1. 每个点找最近的中心
    computeCentroids()     // 2. 每个中心移到所属点的平均位置
    computeCost()          // 3. 每一类：所有点到中心的距离之和
}
```

每一步的工作量（每轮迭代）：

| 函数 | 做了什么 | 计算量 |
|---|---|---|
| `computeAssignments` | 对每个点，算到 K 个中心的距离，取最小 | M × K × N 次乘加 = 3 亿 |
| `computeCentroids` | 把每个点的坐标加到它所属的中心上，再除以个数 | M × N 次加法 = 1 亿 |
| `computeCost` | 对每个点，算到所属中心的距离 | M × N 次乘加 = 1 亿 |

`dist()` 是普通的欧氏距离：`sqrt(Σ (x[n] - y[n])^2)`。

## 准备与运行

```bash
make
make data                # 生成 data.dat（约 800 MB，首次运行需要）
./kmeans                 # 输出 [Total Time]
python3 plot.py          # 可选：画出 start.log / end.log 的聚类结果
```

数据集和收敛条件是固定的，所以每次运行的迭代次数也相同，时间可以直接比较。

## 要做的事与提示

### 1. 先测量：哪个函数最慢

在 [kmeansThread.cpp](kmeansThread.cpp) 的主循环里给三个函数分别计时，累加所有迭代的时间：

```cpp
#include "CycleTimer.h"     // 已在 main.cpp 中使用，路径见 ../common

double tA = 0, tC = 0, tK = 0;
while (...) {
    ...
    double t0 = CycleTimer::currentSeconds();
    computeAssignments(&args);
    double t1 = CycleTimer::currentSeconds();
    computeCentroids(&args);
    double t2 = CycleTimer::currentSeconds();
    computeCost(&args);
    double t3 = CycleTimer::currentSeconds();
    tA += t1 - t0;  tC += t2 - t1;  tK += t3 - t2;
    iter++;
}
printf("assign %.1f ms, centroids %.1f ms, cost %.1f ms, iters %d\n",
       tA*1000, tC*1000, tK*1000, iter);
```

在动手优化之前，先把三个时间的占比写下来。用 Amdahl 定律估一下：如果只把最慢的那个函数加速到 `T` 倍，总加速比最多是多少？

### 2. 只并行化最慢的那部分

提示：最慢的通常是 `computeAssignments`，它的计算量是其他两个的 K 倍。**以你自己的测量为准。**

看一下起始代码的写法：外层循环是 `k`（中心），内层是 `m`（点）。`WorkerArgs` 里的 `start` 和 `end` 是按 **k** 来切分的。但 K 只有 3，按 k 切最多只能用 3 个线程，而且不同 k 的线程会同时写同一个 `minDist[m]` 和 `clusterAssignments[m]`，产生数据竞争。

更好的切分方式是**按点切**：每个点找最近中心的过程和其他点完全独立。

伪代码：

```
// 每个线程负责 [mStart, mEnd) 范围的点
void assignRange(args, mStart, mEnd) {
    for (m = mStart; m < mEnd; m++) {
        best = -1, bestDist = 1e30
        for (k = 0; k < K; k++) {
            d = dist(&data[m*N], &centroids[k*N], N)
            if (d < bestDist) { bestDist = d; best = k; }
        }
        clusterAssignments[m] = best      // 只写自己负责的点，没有竞争
    }
}

// 主循环里：
for (t = 0; t < numThreads; t++)
    threads[t] = std::thread(assignRange, &args, t*M/T, (t+1)*M/T)
for (t ...) threads[t].join()
```

这样做的好处：

- 每个点的 `minDist` 只是一个局部变量，不需要再分配 M 个 double 的数组。
- 内外层循环交换后，同一个点的数据 `data[m*N .. m*N+N]` 连续读 K 次，缓存命中更好。
- 每个线程写的 `clusterAssignments` 区间不重叠，不需要锁。

注意事项：

- 只能用 `std::thread`，不能用 ISPC 或 OpenMP（和原题一致）。
- 线程数建议从逻辑核数开始试：`std::thread::hardware_concurrency()`。
- 每轮迭代都创建和销毁线程有开销，但和 3 亿次乘加相比很小，本题不必优化这一点。
- `std::thread` 需要 `#include <thread>`。Makefile 已经链接了 `-lpthread`，不需要改。

### 3. 验证结果正确

并行化不应该改变计算结果：

- 迭代次数应该和串行版本相同（第 1 步的 `iters`）。
- 用 `python3 plot.py` 对比 `end.log`，聚类结果应该一样。
- 如果迭代次数变了，多半是线程负责的范围有重叠或遗漏，或者有两个线程写了同一个位置。

### 4. 再看一遍剩下的时间

`computeAssignments` 加速以后，另外两个函数的占比变大了。把第 1 步的计时再跑一遍：

- 总加速比和你用 Amdahl 定律估的上限接近吗？
- 本题只要求加速 `computeAssignments`。如果还想继续，`computeCost` 的结构和它很像（每个点独立地算一次距离，然后**累加**到 `accum[k]`）。累加有竞争，该怎么处理？提示：每个线程先算自己的部分和，最后再合并。

## 目标

原题要求在 myth 机器（4 核 8 线程）上达到约 2.1x 以上。在 275HX（24 核）或 i5-11300H（4 核 8 线程）上，只要按点并行化了 `computeAssignments` 并给出计时数据，加速比应该明显超过这个值。提交时附上第 1 步和第 4 步的计时表格即可。
