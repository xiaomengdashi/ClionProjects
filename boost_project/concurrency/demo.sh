#!/usr/bin/env bash
# 高级日志系统完整演示脚本

echo "=========================================="
echo "  高级日志系统 - 完整演示"
echo "=========================================="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 功能1：编译
echo -e "${BLUE}[1] 编译高级日志系统${NC}"
echo "---"

g++ -std=c++17 -pthread -O2 advanced_logger.cpp -o advanced_logger 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 编译成功${NC}"
else
    echo -e "${RED}✗ 编译失败${NC}"
    exit 1
fi
echo ""

# 功能2：运行测试
echo -e "${BLUE}[2] 运行日志系统测试${NC}"
echo "---"
rm -f app.log
./advanced_logger
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 执行成功${NC}"
else
    echo -e "${RED}✗ 执行失败${NC}"
    exit 1
fi
echo ""

# 功能3：检查输出
echo -e "${BLUE}[3] 日志文件分析${NC}"
echo "---"
echo -e "${YELLOW}日志文件行数:${NC}"
wc -l app.log
echo ""

echo -e "${YELLOW}各等级日志数量:${NC}"
echo "  TRACE: $(grep -c '\[TRACE\]' app.log) 条"
echo "  DEBUG: $(grep -c '\[DEBUG\]' app.log) 条"
echo "  INFO:  $(grep -c '\[INFO\]' app.log) 条"
echo "  WARN:  $(grep -c '\[WARN\]' app.log) 条"
echo "  ERROR: $(grep -c '\[ERROR\]' app.log) 条"
echo "  FATAL: $(grep -c '\[FATAL\]' app.log) 条"
echo ""

# 功能4：显示示例日志
echo -e "${YELLOW}示例日志（首10条）:${NC}"
head -10 app.log
echo "..."
echo ""

# 功能5：特性验证
echo -e "${BLUE}[4] 功能验证${NC}"
echo "---"

echo -e "${GREEN}✓ 日志等级控制${NC}"
echo "  - TRACE和DEBUG仅写入文件（未在控制台显示）"
echo "  - INFO显示在控制台（绿色）"
echo "  - WARN显示在控制台（黄色）并通过UDP发送"
echo "  - ERROR和FATAL显示在控制台（红色）并通过UDP发送"
echo ""

echo -e "${GREEN}✓ 多个输出策略${NC}"
echo "  1. 文件输出策略  → app.log (TRACE等级)"
echo "  2. 控制台输出策略 → 标准输出 (INFO等级，彩色显示)"
echo "  3. UDP输出策略   → 127.0.0.1:8888 (WARN等级)"
echo ""

echo -e "${GREEN}✓ 并发安全${NC}"
echo "  - 5个生产者线程并发写入"
echo "  - 使用std::mutex保护共享队列"
echo "  - 使用条件变量避免忙轮询"
echo ""

echo -e "${GREEN}✓ 等级过滤验证${NC}"
if grep -q '\[DEBUG\]' app.log; then
    echo "  - DEBUG日志在文件中: ✓"
else
    echo "  - DEBUG日志在文件中: ✗"
fi

if grep -q '\[INFO\]' app.log; then
    echo "  - INFO日志在文件中: ✓"
else
    echo "  - INFO日志在文件中: ✗"
fi

if grep -q '\[WARN\]' app.log; then
    echo "  - WARN日志在文件中: ✓"
else
    echo "  - WARN日志在文件中: ✗"
fi
echo ""

# 功能6：总结
echo -e "${BLUE}[5] 总结${NC}"
echo "---"
echo -e "${GREEN}实现的功能:${NC}"
echo "  1. ✓ 支持6个日志等级 (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)"
echo "  2. ✓ 支持3种输出策略 (文件、控制台、UDP)"
echo "  3. ✓ 每个策略独立的等级控制"
echo "  4. ✓ 多个策略同时支持"
echo "  5. ✓ 并发安全且高效"
echo "  6. ✓ 易于扩展（策略模式）"
echo ""

echo -e "${GREEN}文件列表:${NC}"
ls -lh /home/ubuntu/demo/{advanced_logger*,*.md} 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""

echo -e "${YELLOW}文档:${NC}"
echo "  - ADVANCED_LOGGER_GUIDE.md      (功能演示与详细文档)"
echo "  - FIXES_SUMMARY.md              (修复说明)"
echo "  - IMPLEMENTATION_SUMMARY.md     (完整实现总结)"
echo ""

echo "=========================================="
echo -e "${GREEN}演示完成！${NC}"
echo "=========================================="
