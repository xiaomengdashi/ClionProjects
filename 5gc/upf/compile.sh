#!/bin/bash
# 5G UPF 编译脚本
# 功能：支持 src 目录下的文件编译，输出到 bin 文件夹
# 用法：
#   ./compile.sh              # 编译所有文件
#   ./compile.sh clean        # 清空 bin 文件夹
#   ./compile.sh upf_example  # 编译指定文件

# 注：不使用 set -e，以便继续编译其他文件即使有错误

# ============= 定义变量 =============
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
BIN_DIR="${SCRIPT_DIR}/bin"
BUILD_LOG="${BIN_DIR}/build.log"

# ============= 颜色输出定义 =============
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============= 函数定义 =============

print_header() {
    echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║ 5G UPF Compilation Script              ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
    echo ""
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

# 初始化编译环境
init_build_env() {
    print_info "初始化编译环境..."

    # 创建 bin 目录
    if [ ! -d "$BIN_DIR" ]; then
        mkdir -p "$BIN_DIR"
        print_success "创建 bin 目录: $BIN_DIR"
    fi

    # 创建 src 目录
    if [ ! -d "$SRC_DIR" ]; then
        mkdir -p "$SRC_DIR"
        print_warn "src 目录不存在，已创建"
        print_warn "请将 *.c 文件放到 $SRC_DIR 目录中"
    fi

    # 清空日志
    > "$BUILD_LOG"
}

# 清理编译输出
clean_build() {
    print_info "清理编译输出..."
    if [ -d "$BIN_DIR" ]; then
        find "$BIN_DIR" -maxdepth 1 -type f ! -name "build.log" -delete
        print_success "已清空 bin 目录"
    fi
}

# 检查依赖
check_dependencies() {
    print_info "检查编译依赖..."

    # 检查 gcc
    if ! command -v gcc &> /dev/null; then
        print_error "gcc 未安装"
        exit 1
    fi
    print_success "gcc 已安装"

    # 检查 DPDK
    if pkg-config --exists libdpdk; then
        DPDK_VERSION=$(pkg-config --modversion libdpdk)
        print_success "DPDK 已安装 (版本: $DPDK_VERSION)"
        DPDK_AVAILABLE=1
    else
        print_warn "DPDK 未安装 - 跳过 DPDK 相关编译"
        DPDK_AVAILABLE=0
    fi
}

# 编译单个文件
compile_file() {
    local src_file="$1"
    local bin_name="$(basename "$src_file" .c)"
    local bin_file="$BIN_DIR/$bin_name"

    # 跳过不存在的文件
    if [ ! -f "$src_file" ]; then
        return 1
    fi

    # 检查是否需要 DPDK
    if grep -q "rte_" "$src_file" 2>/dev/null; then
        if [ $DPDK_AVAILABLE -eq 0 ]; then
            print_warn "跳过 $bin_name (需要 DPDK 但未安装)"
            return 0
        fi

        print_info "编译 $bin_name (DPDK)..."
        if gcc -o "$bin_file" "$src_file" \
            $(pkg-config --cflags --libs libdpdk) \
            -Wall -Wextra -O2 >> "$BUILD_LOG" 2>&1; then
            print_success "$bin_name 编译成功 -> $bin_file"
            return 0
        else
            print_error "$bin_name 编译失败"
            tail -20 "$BUILD_LOG" | sed 's/^/  /'
            return 1
        fi
    else
        # 不需要 DPDK 的程序
        print_info "编译 $bin_name (标准库)..."
        if gcc -o "$bin_file" "$src_file" \
            -lm -Wall -Wextra -O2 >> "$BUILD_LOG" 2>&1; then
            print_success "$bin_name 编译成功 -> $bin_file"
            return 0
        else
            print_error "$bin_name 编译失败"
            tail -20 "$BUILD_LOG" | sed 's/^/  /'
            return 1
        fi
    fi
}

# 编译所有文件
compile_all() {
    print_info "扫描 $SRC_DIR 目录..."

    local total=0
    local success=0
    local failed=0

    # 查找所有 .c 文件
    for src_file in "$SRC_DIR"/*.c; do
        if [ -f "$src_file" ]; then
            ((total++))
            if compile_file "$src_file"; then
                ((success++))
            else
                ((failed++))
            fi
            echo ""
        fi
    done

    # 也检查当前目录的 .c 文件（向后兼容）
    for src_file in *.c; do
        if [ -f "$src_file" ] && [ ! -f "$SRC_DIR/$src_file" ]; then
            ((total++))
            if compile_file "$src_file"; then
                ((success++))
            else
                ((failed++))
            fi
            echo ""
        fi
    done

    # 打印总结
    print_info "编译总结"
    echo "  总计: $total 个文件"
    echo "  成功: $success 个"
    if [ $failed -gt 0 ]; then
        echo -e "  ${RED}失败: $failed 个${NC}"
    fi
}

# 显示帮助信息
show_help() {
    cat << EOF
用法: $0 [COMMAND] [OPTIONS]

命令:
  (无参数)          编译所有文件
  clean             清空 bin 目录
  <filename>        编译指定的 C 文件
  help              显示此帮助信息

选项:
  -v, --verbose     详细输出
  -q, --quiet       安静模式

目录结构:
  src/              源文件目录 (存放 *.c 文件)
  bin/              输出目录 (编译后的可执行文件)

示例:
  $0                    # 编译所有文件
  $0 clean              # 清空编译输出
  $0 upf_example        # 编译 src/upf_example.c

编译日志:
  $BUILD_LOG

EOF
}

# ============= 主程序开始 =============

print_header

# 解析命令行参数
case "${1:-}" in
    "clean")
        init_build_env
        clean_build
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    "")
        # 默认编译所有文件
        init_build_env
        check_dependencies
        echo ""
        compile_all
        echo ""
        print_info "编译日志已保存到: $BUILD_LOG"
        ;;
    *)
        # 编译指定的文件
        init_build_env
        check_dependencies
        echo ""

        # 尝试从 src 目录编译
        if [ -f "$SRC_DIR/$1.c" ]; then
            compile_file "$SRC_DIR/$1.c"
        # 尝试从当前目录编译
        elif [ -f "$1.c" ]; then
            compile_file "$1.c"
        # 尝试直接使用提供的文件名
        elif [ -f "$1" ]; then
            compile_file "$1"
        else
            print_error "找不到文件: $1"
            print_info "请检查文件是否存在于 $SRC_DIR 或当前目录"
            exit 1
        fi
        ;;
esac

echo ""
print_info "完成"
