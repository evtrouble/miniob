#!/bin/bash
# 运行 sysbench 并生成火焰图的脚本
# 使用方法: ./test/perf/run_sysbench_with_flamegraph.sh [test_case] [threads] [duration] [thread_model]
# 参数说明:
#   test_case: sysbench 测试用例 (默认: miniob_select)
#   threads: sysbench 线程数 (默认: 10)
#   duration: 运行时间（秒）(默认: 30)
#   thread_model: observer 线程模型 (默认: one-thread-per-connection)
#                 可选值: one-thread-per-connection, java-thread-pool

set -e

# 默认参数
TEST_CASE=${1:-miniob_select}
THREADS=${2:-10}
DURATION=${3:-30}  # 运行时间（秒）
THREAD_MODEL=${4:-one-thread-per-connection}  # 线程模型

# 验证线程模型
if [ "$THREAD_MODEL" != "one-thread-per-connection" ] && [ "$THREAD_MODEL" != "java-thread-pool" ]; then
    echo "错误: 无效的线程模型: $THREAD_MODEL"
    echo "可选值: one-thread-per-connection, java-thread-pool"
    exit 1
fi

# 检查依赖
if ! command -v perf &> /dev/null; then
    echo "错误: 需要安装 perf"
    echo "Ubuntu/Debian: sudo apt-get install linux-perf"
    echo "CentOS/RHEL: sudo yum install perf"
    exit 1
fi

if ! command -v sysbench &> /dev/null; then
    echo "错误: 需要安装 sysbench"
    echo "安装方法: curl -s https://packagecloud.io/install/repositories/akopytov/sysbench/script.deb.sh | sudo bash"
    echo "          sudo apt-get install sysbench"
    exit 1
fi

# 检查 FlameGraph 工具
FLAMEGRAPH_DIR="./tools/FlameGraph"
if [ ! -d "$FLAMEGRAPH_DIR" ]; then
    echo "下载 FlameGraph 工具..."
    mkdir -p tools
    cd tools
    git clone https://github.com/brendangregg/FlameGraph.git || true
    cd ..
fi

# 检查 observer 是否已编译
# 优先使用 debug 版本，因为 debug 版本有完整的符号信息，火焰图更清晰
OBSERVER_PATH="./build_debug/bin/observer"
BUILD_TYPE="debug"
if [ ! -f "$OBSERVER_PATH" ]; then
    OBSERVER_PATH="./build_release/bin/observer"
    BUILD_TYPE="release"
    if [ ! -f "$OBSERVER_PATH" ]; then
        echo "错误: 找不到 observer 可执行文件"
        echo "建议使用 debug 版本以获得更清晰的火焰图:"
        echo "  bash build.sh debug -DCONCURRENCY=ON -DWITH_UNIT_TESTS=OFF -DWITH_BENCHMARK=OFF -DENABLE_ASAN=OFF -DWITH_MEMTRACER=OFF --make -j6"
        echo "或者使用 release 版本:"
        echo "  bash build.sh release -DCONCURRENCY=ON -DWITH_UNIT_TESTS=OFF -DWITH_BENCHMARK=OFF -DENABLE_ASAN=OFF -DWITH_MEMTRACER=OFF --make -j6"
        exit 1
    fi
    echo "警告: 使用 release 版本，火焰图可能显示很多 [unknown]"
    echo "建议使用 debug 版本以获得更清晰的火焰图"
fi

echo "=========================================="
echo "Sysbench 火焰图生成脚本"
echo "=========================================="
echo "测试用例: $TEST_CASE"
echo "线程数: $THREADS"
echo "运行时间: ${DURATION}秒"
echo "线程模型: $THREAD_MODEL"
echo "编译类型: $BUILD_TYPE"
echo "Observer 路径: $OBSERVER_PATH"
echo "=========================================="

# 清理之前的文件
rm -f /tmp/miniob.sock perf.data perf.data.old flamegraph.svg

# 启动 observer（后台运行）
echo "启动 observer (线程模型: $THREAD_MODEL)..."
nohup $OBSERVER_PATH -T $THREAD_MODEL -s /tmp/miniob.sock -f etc/observer.ini -P mysql -t mvcc -d disk > /tmp/observer.log 2>&1 &
OBSERVER_PID=$!
echo "Observer PID: $OBSERVER_PID"

# 等待 observer 启动
sleep 3

# 检查 observer 是否启动成功
if ! mysql -S /tmp/miniob.sock -e "show tables" &> /dev/null; then
    echo "错误: observer 启动失败，请检查日志: /tmp/observer.log"
    kill $OBSERVER_PID 2>/dev/null || true
    exit 1
fi

echo "Observer 启动成功"

# 准备数据
echo "准备 sysbench 数据..."
cd test/sysbench
sysbench --mysql-socket=/tmp/miniob.sock --mysql-ignore-errors=41 --threads=1 $TEST_CASE prepare

# 使用 perf 记录性能数据（在后台运行）
# 使用 --call-graph dwarf 获取更好的调用栈信息（需要 debug 版本）
# -g 使用默认的 frame pointer，--call-graph dwarf 使用 DWARF 调试信息（更准确）
echo "开始记录性能数据（${DURATION}秒）..."
cd ../..
if [ "$BUILD_TYPE" = "debug" ]; then
    # debug 版本使用 dwarf 调用图，更准确
    perf record -F 99 --call-graph dwarf -p $OBSERVER_PID -- sleep $DURATION &
else
    # release 版本使用默认的 frame pointer
    perf record -F 99 -g -p $OBSERVER_PID -- sleep $DURATION &
fi
PERF_PID=$!

# 运行 sysbench
echo "运行 sysbench..."
cd test/sysbench
sysbench --mysql-socket=/tmp/miniob.sock --mysql-ignore-errors=41 --threads=$THREADS --time=$DURATION $TEST_CASE run

# 等待 perf 完成
echo "等待 perf 记录完成..."
wait $PERF_PID

# 停止 observer
echo "停止 observer..."
kill $OBSERVER_PID 2>/dev/null || true
sleep 1

# 生成火焰图
echo "生成火焰图..."
cd ../..
perf script | $FLAMEGRAPH_DIR/stackcollapse-perf.pl | $FLAMEGRAPH_DIR/flamegraph.pl > flamegraph.svg

if [ -f "flamegraph.svg" ]; then
    echo "=========================================="
    echo "火焰图生成成功: flamegraph.svg"
    echo "=========================================="
    echo "可以用浏览器打开查看:"
    echo "  firefox flamegraph.svg"
    echo "  或"
    echo "  google-chrome flamegraph.svg"
else
    echo "错误: 火焰图生成失败"
    exit 1
fi

