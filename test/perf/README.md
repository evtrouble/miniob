# Sysbench 火焰图生成指南

## 快速开始

```bash
# 基本用法（默认运行 miniob_select，10 线程，30 秒，one-thread-per-connection）
./test/perf/run_sysbench_with_flamegraph.sh

# 指定测试用例
./test/perf/run_sysbench_with_flamegraph.sh miniob_insert

# 指定线程数和运行时间
./test/perf/run_sysbench_with_flamegraph.sh miniob_select 20 60

# 指定线程模型（可选：one-thread-per-connection 或 java-thread-pool）
./test/perf/run_sysbench_with_flamegraph.sh miniob_select 10 30 java-thread-pool
```

### 参数说明

脚本支持 4 个参数（均为可选）：
- `test_case`: sysbench 测试用例（默认: `miniob_select`）
- `threads`: sysbench 线程数（默认: `10`）
- `duration`: 运行时间，单位秒（默认: `30`）
- `thread_model`: observer 线程模型（默认: `one-thread-per-connection`）
  - 可选值：`one-thread-per-connection`、`java-thread-pool`

## 前置要求

1. **安装 perf**:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install linux-perf
   
   # CentOS/RHEL
   sudo yum install perf
   ```

2. **安装 sysbench**:
   ```bash
   curl -s https://packagecloud.io/install/repositories/akopytov/sysbench/script.deb.sh | sudo bash
   sudo apt-get install sysbench mariadb-client
   ```

3. **编译 observer**:
   ```bash
   # 推荐使用 debug 版本，因为 debug 版本有完整的符号信息，火焰图更清晰
   bash build.sh debug -DCONCURRENCY=ON -DWITH_UNIT_TESTS=OFF -DWITH_BENCHMARK=OFF -DENABLE_ASAN=OFF -DWITH_MEMTRACER=OFF --make -j6
   
   # 或使用 release 版本（火焰图可能显示很多 [unknown]）
   bash build.sh release -DCONCURRENCY=ON -DWITH_UNIT_TESTS=OFF -DWITH_BENCHMARK=OFF -DENABLE_ASAN=OFF -DWITH_MEMTRACER=OFF --make -j6
   ```
   
   **注意**：脚本会优先使用 `build_debug/bin/observer`，如果不存在则使用 `build_release/bin/observer`。debug 版本会使用 `--call-graph dwarf` 获取更准确的调用栈信息。

## 可用的测试用例

- `miniob_select`: SELECT 查询测试
- `miniob_insert`: INSERT 插入测试
- `miniob_delete`: DELETE 删除测试

## 手动运行（更灵活）

如果你想更精细地控制，可以手动运行：

```bash
# 1. 启动 observer（选择线程模型）
# 使用 one-thread-per-connection
./build_debug/bin/observer -T one-thread-per-connection \
  -s /tmp/miniob.sock -f etc/observer.ini -P mysql -t mvcc -d disk &
# 或使用 java-thread-pool
# ./build_debug/bin/observer -T java-thread-pool \
#   -s /tmp/miniob.sock -f etc/observer.ini -P mysql -t mvcc -d disk &
OBSERVER_PID=$!

# 2. 等待启动
sleep 3

# 3. 准备数据
cd test/sysbench
sysbench --mysql-socket=/tmp/miniob.sock --mysql-ignore-errors=41 --threads=1 miniob_select prepare

# 4. 在另一个终端运行 perf（记录 60 秒）
# debug 版本使用 --call-graph dwarf 获取更准确的调用栈
perf record -F 99 --call-graph dwarf -p $OBSERVER_PID -- sleep 60
# release 版本使用 -g（frame pointer）
# perf record -F 99 -g -p $OBSERVER_PID -- sleep 60

# 5. 运行 sysbench（在第一个终端）
sysbench --mysql-socket=/tmp/miniob.sock --mysql-ignore-errors=41 --threads=10 --time=60 miniob_select run

# 6. 停止 observer
kill $OBSERVER_PID

# 7. 生成火焰图
cd ../..
perf script | tools/FlameGraph/stackcollapse-perf.pl | \
  tools/FlameGraph/flamegraph.pl > flamegraph.svg
```

## 查看火焰图

生成的 `flamegraph.svg` 可以用浏览器打开：
- Firefox: `firefox flamegraph.svg`
- Chrome: `google-chrome flamegraph.svg`

在火焰图中：
- **X 轴**: 表示采样时间（宽度越大，占用时间越多）
- **Y 轴**: 表示调用栈深度
- **颜色**: 随机分配，用于区分不同函数
- **点击**: 可以放大查看某个函数的详细信息

## 性能分析技巧

1. **查找热点函数**: 寻找最宽的函数（占用 CPU 时间最多）
2. **查看调用链**: 从下往上查看调用栈，了解函数调用关系
3. **对比不同场景**: 运行不同的测试用例，对比火焰图找出差异

## 常见问题

1. **perf 权限问题**:
   ```bash
   # 需要 root 权限或设置 perf_event_paranoid
   sudo sysctl -w kernel.perf_event_paranoid=-1
   ```

2. **找不到符号或显示 [unknown]**:
   ```bash
   # 确保使用 debug 版本以获得完整的符号信息
   bash build.sh debug -DCONCURRENCY=ON -DWITH_UNIT_TESTS=OFF -DWITH_BENCHMARK=OFF -DENABLE_ASAN=OFF -DWITH_MEMTRACER=OFF --make -j6
   ```
   
   **说明**：debug 版本会使用 `--call-graph dwarf` 获取更准确的调用栈信息，而 release 版本使用 frame pointer，可能显示很多 `[unknown]`。

3. **火焰图为空**:
   - 检查 observer 是否正常运行
   - 检查 perf 是否成功记录数据
   - 确保运行时间足够长（至少 10 秒）

