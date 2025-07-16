#!/bin/bash

# 高级构建脚本 - 支持多种构建选项
# 使用方法: ./advanced_build.sh [选项]

set -e

# 默认配置
BUILD_TYPE="Release"
BUILD_TESTS=OFF
BUILD_BENCHMARKS=OFF
BUILD_EXAMPLES=ON
ENABLE_COVERAGE=OFF
CLEAN_BUILD=false
INSTALL_DEPS=false
VERBOSE=false
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印帮助信息
print_help() {
    echo "OpenSSL静态库项目高级构建脚本"
    echo ""
    echo "使用方法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help              显示此帮助信息"
    echo "  -c, --clean             清理构建目录"
    echo "  -d, --debug             Debug构建模式"
    echo "  -r, --release           Release构建模式 (默认)"
    echo "  -t, --tests             启用单元测试"
    echo "  -b, --benchmarks        启用性能基准测试"
    echo "  -e, --examples          启用示例程序 (默认)"
    echo "  --no-examples           禁用示例程序"
    echo "  --coverage              启用代码覆盖率"
    echo "  --install-deps          安装依赖项"
    echo "  -v, --verbose           详细输出"
    echo "  -j, --jobs N            并行编译任务数 (默认: $PARALLEL_JOBS)"
    echo ""
    echo "示例:"
    echo "  $0 --clean --tests --benchmarks"
    echo "  $0 --debug --coverage"
    echo "  $0 --release --install-deps"
}

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."
    
    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake未安装"
        exit 1
    fi
    
    # 检查编译器
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        log_error "未找到C++编译器"
        exit 1
    fi
    
    # 检查OpenSSL
    if ! pkg-config --exists openssl; then
        log_warning "OpenSSL pkg-config未找到，可能需要手动指定路径"
    fi
    
    log_success "依赖检查完成"
}

# 安装依赖项
install_dependencies() {
    log_info "安装依赖项..."
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        if command -v brew &> /dev/null; then
            brew install openssl cmake
            if [[ "$BUILD_TESTS" == "ON" ]]; then
                brew install googletest
            fi
            if [[ "$BUILD_BENCHMARKS" == "ON" ]]; then
                brew install google-benchmark
            fi
        else
            log_error "请先安装Homebrew"
            exit 1
        fi
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y libssl-dev cmake build-essential
            if [[ "$BUILD_TESTS" == "ON" ]]; then
                sudo apt-get install -y libgtest-dev
            fi
            if [[ "$BUILD_BENCHMARKS" == "ON" ]]; then
                sudo apt-get install -y libbenchmark-dev
            fi
        elif command -v yum &> /dev/null; then
            sudo yum install -y openssl-devel cmake gcc-c++
        else
            log_error "不支持的Linux发行版"
            exit 1
        fi
    else
        log_error "不支持的操作系统: $OSTYPE"
        exit 1
    fi
    
    log_success "依赖项安装完成"
}

# 清理构建目录
clean_build() {
    log_info "清理构建目录..."
    if [ -d "build" ]; then
        rm -rf build
        log_success "构建目录已清理"
    else
        log_info "构建目录不存在，跳过清理"
    fi
}

# 配置构建
configure_build() {
    log_info "配置构建..."
    
    mkdir -p build
    cd build
    
    CMAKE_ARGS=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DBUILD_TESTS=$BUILD_TESTS"
        "-DBUILD_BENCHMARKS=$BUILD_BENCHMARKS"
        "-DBUILD_EXAMPLES=$BUILD_EXAMPLES"
        "-DENABLE_COVERAGE=$ENABLE_COVERAGE"
    )
    
    if [[ "$VERBOSE" == "true" ]]; then
        CMAKE_ARGS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    log_info "CMake配置: ${CMAKE_ARGS[*]}"
    cmake "${CMAKE_ARGS[@]}" ..
    
    cd ..
    log_success "构建配置完成"
}

# 执行构建
build_project() {
    log_info "开始构建项目..."
    
    cd build
    
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --build . --parallel $PARALLEL_JOBS --verbose
    else
        cmake --build . --parallel $PARALLEL_JOBS
    fi
    
    cd ..
    log_success "项目构建完成"
}

# 运行测试
run_tests() {
    if [[ "$BUILD_TESTS" == "ON" ]]; then
        log_info "运行单元测试..."
        cd build
        ctest --output-on-failure
        cd ..
        log_success "单元测试完成"
    fi
}

# 运行基准测试
run_benchmarks() {
    if [[ "$BUILD_BENCHMARKS" == "ON" ]]; then
        log_info "运行性能基准测试..."
        if [ -f "build/benchmarks/crypto_benchmark" ]; then
            ./build/benchmarks/crypto_benchmark
            log_success "基准测试完成"
        else
            log_warning "基准测试可执行文件未找到"
        fi
    fi
}

# 生成覆盖率报告
generate_coverage() {
    if [[ "$ENABLE_COVERAGE" == "ON" ]]; then
        log_info "生成代码覆盖率报告..."
        if command -v gcov &> /dev/null && command -v lcov &> /dev/null; then
            cd build
            lcov --capture --directory . --output-file coverage.info
            lcov --remove coverage.info '/usr/*' --output-file coverage.info
            lcov --list coverage.info
            cd ..
            log_success "覆盖率报告生成完成"
        else
            log_warning "gcov或lcov未安装，跳过覆盖率报告生成"
        fi
    fi
}

# 显示构建结果
show_results() {
    log_info "构建结果:"
    echo ""
    
    if [ -f "build/lib/libProjectA.a" ]; then
        echo "✓ ProjectA静态库: build/lib/libProjectA.a"
        ls -lh build/lib/libProjectA.a
    fi
    
    if [ -f "build/bin/ProjectB" ]; then
        echo "✓ ProjectB可执行文件: build/bin/ProjectB"
        ls -lh build/bin/ProjectB
    fi
    
    if [[ "$BUILD_TESTS" == "ON" ]] && [ -f "build/tests/crypto_tests" ]; then
        echo "✓ 单元测试: build/tests/crypto_tests"
    fi
    
    if [[ "$BUILD_BENCHMARKS" == "ON" ]] && [ -f "build/benchmarks/crypto_benchmark" ]; then
        echo "✓ 基准测试: build/benchmarks/crypto_benchmark"
    fi
    
    if [[ "$BUILD_EXAMPLES" == "ON" ]] && [ -f "build/examples/crypto_examples" ]; then
        echo "✓ 示例程序: build/examples/crypto_examples"
    fi
    
    echo ""
    log_success "构建完成！"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_help
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -t|--tests)
            BUILD_TESTS=ON
            shift
            ;;
        -b|--benchmarks)
            BUILD_BENCHMARKS=ON
            shift
            ;;
        -e|--examples)
            BUILD_EXAMPLES=ON
            shift
            ;;
        --no-examples)
            BUILD_EXAMPLES=OFF
            shift
            ;;
        --coverage)
            ENABLE_COVERAGE=ON
            BUILD_TYPE="Debug"  # 覆盖率需要Debug模式
            shift
            ;;
        --install-deps)
            INSTALL_DEPS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        *)
            log_error "未知选项: $1"
            print_help
            exit 1
            ;;
    esac
done

# 主执行流程
main() {
    log_info "OpenSSL静态库项目高级构建脚本"
    log_info "构建类型: $BUILD_TYPE"
    log_info "并行任务数: $PARALLEL_JOBS"
    
    if [[ "$INSTALL_DEPS" == "true" ]]; then
        install_dependencies
    fi
    
    check_dependencies
    
    if [[ "$CLEAN_BUILD" == "true" ]]; then
        clean_build
    fi
    
    configure_build
    build_project
    run_tests
    run_benchmarks
    generate_coverage
    show_results
}

# 执行主函数
main