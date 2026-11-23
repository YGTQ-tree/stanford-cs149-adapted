# Program 1: 用线程并行生成分形（20 分）

> 本题考查**多核并行（线程级并行）**与**负载均衡**。主 README 的 Program 1 一节是同一份要求，这里多了背景和提示。

## 背景：核心运算是什么

Mandelbrot 集合是复平面上的一组点 `c`。判定方法是反复迭代

```
z_0 = c
z_{n+1} = z_n^2 + c
```

如果 `|z|` 一直不超过 2，就认为 `c` 在集合里；一旦 `|z|^2 > 4`，这个点就“逃逸”了。程序记录的是**逃逸前迭代了多少次**（上限 `maxIterations = 256`），这个次数就是像素的亮度。

```c
// mandelbrotSerial.cpp 里的 mandel()，每个像素调用一次
int mandel(float c_re, float c_im, int count) {
    float z_re = c_re, z_im = c_im;
    for (i = 0; i < count; ++i) {
        if (z_re*z_re + z_im*z_im > 4.f) break;   // 逃逸
        float new_re = z_re*z_re - z_im*z_im;      // (a+bi)^2 的实部
        float new_im = 2.f * z_re * z_im;          // (a+bi)^2 的虚部
        z_re = c_re + new_re;
        z_im = c_im + new_im;
    }
    return i;                                       // 亮度 = 迭代次数
}
```

图像大小是 1600 × 1200，像素 `(i, j)` 对应复平面上的点 `x = x0 + i*dx, y = y0 + j*dy`。

和并行有关的两个性质：

- **每个像素彼此独立**：没有数据依赖，所以可以任意切分给线程。
- **每个像素的计算量差别很大**：集合内部的点要跑满 256 次，远处的点 1、2 次就逃逸。图像里亮的地方就是计算量大的地方。这正是负载不均衡的来源。

`mandelbrotSerial(x0, y0, x1, y1, width, height, startRow, numRows, maxIterations, output)` 只计算从 `startRow` 开始的 `numRows` 行，你可以直接拿它当“每个线程的工作单元”。

## 运行

```bash
make
./mandelbrot              # 默认 2 线程, VIEW 1
./mandelbrot -t 8         # 8 线程
./mandelbrot -t 8 -v 2    # VIEW 2
```

程序会输出串行和线程版本的时间与加速比，并检查两者的输出是否一致（起始代码会报 mismatch，这是正常的）。

## 要做的事与提示

你只需要改 [mandelbrotThread.cpp](mandelbrotThread.cpp)。

### 1. 两个线程：上半幅 / 下半幅

主线程自己当线程 0，另外再创建一个 `std::thread` 作为线程 1，这部分起始代码已经写好。你只要在 `workerThreadStart()` 里根据 `threadId` 算出自己负责的行：

```
rowsPerThread = height / numThreads
startRow      = threadId * rowsPerThread
mandelbrotSerial(..., startRow, rowsPerThread, ...)
```

### 2. 推广到任意线程数，画加速比图

- 上面的公式在 `height` 不能被 `numThreads` 整除时会漏行（例如 1200 / 7）。让**最后一个线程**把余下的行也算掉：`numRows = (threadId == numThreads-1) ? height - startRow : rowsPerThread`。
- 用一个 shell 循环扫描线程数，把结果记下来：

  ```bash
  for t in 2 3 4 6 8 16 24; do ./mandelbrot -t $t | grep speedup; done
  ```

- 看图时想一想：3 个线程时，**中间那条带**对应图像的哪个区域？它亮不亮？

### 3. 给每个线程计时

在 `workerThreadStart()` 的开头和结尾各取一次时间：

```cpp
double t0 = CycleTimer::currentSeconds();
// ... mandelbrotSerial(...) ...
double t1 = CycleTimer::currentSeconds();
printf("[thread %d] %.3f ms\n", args->threadId, (t1 - t0) * 1000);
```

如果加速比低，你应该会看到**少数几个线程用时远长于其他线程**。整体用时由最慢的线程决定。

### 4. 改进映射，让两个视图都接近线性

目标是让每个线程分到的“亮区”和“暗区”差不多多。思路：**不要让每个线程拿一整块连续的行，而是让它拿分散在整幅图里的行。**

伪代码（交错 / cyclic 分配）：

```
for (row = threadId; row < height; row += numThreads)
    计算第 row 行
```

为什么有效：相邻的行计算量很接近，交错之后每个线程都拿到了亮区和暗区的一部分。

可以进一步思考：每次拿 1 行和每次拿 k 行（block-cyclic）有什么区别？对缓存有影响吗？

要求：不使用锁、原子变量等线程间同步，也不对某个线程数写死特殊逻辑。

### 5. 线程数增加到 2T

用 `scripts/machine_info.sh` 查看物理核数和逻辑核数。思考：

- 线程数超过逻辑核数后，操作系统只能让多个线程轮流使用同一个逻辑核。还会有新的计算资源吗？
- 超线程（i5-11300H）是两个线程**共享**一个核的执行单元。Mandelbrot 主要瓶颈是计算还是访存？共享执行单元能帮上多少？

### 6. P 核 / E 核与超线程

- 275HX 有 8 个 P 核和 16 个 E 核，E 核单线程更慢。均分行数时，分到 E 核的线程会更晚结束吗？结合第 3 问的计时数据回答。
- 第 4 问的交错分配是**静态**的。如果线程本身速度不同，静态分配还够吗？（可以提一下动态分配，例如用原子计数器取下一行，但本题不允许同步，只需讨论。）
- WSL2 下看不到哪个逻辑核是 P 核、哪个是 E 核，所以只能从计时数据推断。

## 常见错误

- 线程负责的行数算错，导致漏行或重复计算，程序会报 mismatch。
- 在 `workerThreadStart()` 里留着 `printf("Hello world ...")`，会影响计时。
- 每个线程都调用了一遍完整图像，结果对了但没有加速。
