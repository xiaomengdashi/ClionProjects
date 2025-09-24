#!/bin/bash

# StdHTTPS 构建脚本
# 自动检测系统依赖并构建项目

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[信息]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[成功]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[警告]${NC} $1"
}

print_error() {
    echo -e "${RED}[错误]${NC} $1"
}

# 检测操作系统
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [[ -f /etc/debian_version ]]; then
            OS="debian"
        elif [[ -f /etc/redhat-release ]]; then
            OS="redhat"
        else
            OS="linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    else
        OS="unknown"
    fi
    
    print_info "检测到操作系统: $OS"
}

# 检查命令是否存在
check_command() {
    if ! command -v $1 &> /dev/null; then
        return 1
    fi
    return 0
}

# 安装依赖
install_dependencies() {
    print_info "检查并安装依赖..."
    
    case $OS in
        "debian")
            if ! check_command cmake; then
                print_info "安装cmake..."
                sudo apt-get update
                sudo apt-get install -y cmake
            fi
            
            if ! check_command g++; then
                print_info "安装构建工具..."
                sudo apt-get install -y build-essential
            fi
            
            if ! pkg-config --exists openssl; then
                print_info "安装OpenSSL开发包..."
                sudo apt-get install -y libssl-dev
            fi
            ;;
            
        "redhat")
            if ! check_command cmake; then
                print_info "安装cmake..."
                sudo yum install -y cmake
            fi
            
            if ! check_command g++; then
                print_info "安装构建工具..."
                sudo yum groupinstall -y "Development Tools"
            fi
            
            if ! pkg-config --exists openssl; then
                print_info "安装OpenSSL开发包..."
                sudo yum install -y openssl-devel
            fi
            ;;
            
        "macos")
            if ! check_command cmake; then
                print_info "请安装cmake: brew install cmake"
                exit 1
            fi
            
            if ! check_command g++; then
                print_info "请安装Xcode命令行工具: xcode-select --install"
                exit 1
            fi
            
            if ! pkg-config --exists openssl; then
                print_warning "可能需要安装OpenSSL: brew install openssl"
            fi
            ;;
            
        *)
            print_warning "未识别的操作系统，请手动安装依赖"
            ;;
    esac
}

# 检查依赖
check_dependencies() {
    print_info "检查依赖项..."
    
    local missing_deps=()
    
    if ! check_command cmake; then
        missing_deps+=("cmake")
    fi
    
    if ! check_command g++; then
        missing_deps+=("g++")
    fi
    
    if ! pkg-config --exists openssl; then
        missing_deps+=("openssl-dev")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "缺少依赖项: ${missing_deps[*]}"
        print_info "尝试自动安装..."
        install_dependencies
    else
        print_success "所有依赖项已满足"
    fi
}

# 创建构建目录
create_build_dir() {
    print_info "创建构建目录..."
    
    if [ -d "build" ]; then
        print_warning "构建目录已存在，清理中..."
        rm -rf build
    fi
    
    mkdir build
    cd build
}

# 配置项目
configure_project() {
    print_info "配置CMake项目..."
    
    local cmake_args=""
    
    # 根据构建类型设置参数
    if [ "$BUILD_TYPE" = "Debug" ]; then
        cmake_args="-DCMAKE_BUILD_TYPE=Debug"
    elif [ "$BUILD_TYPE" = "Release" ]; then
        cmake_args="-DCMAKE_BUILD_TYPE=Release"
    else
        cmake_args="-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    fi
    
    # macOS特殊处理OpenSSL路径
    if [ "$OS" = "macos" ]; then
        if [ -d "/usr/local/opt/openssl" ]; then
            cmake_args="$cmake_args -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl"
        elif [ -d "/opt/homebrew/opt/openssl" ]; then
            cmake_args="$cmake_args -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl"
        fi
    fi
    
    cmake $cmake_args ..
    
    if [ $? -eq 0 ]; then
        print_success "CMake配置成功"
    else
        print_error "CMake配置失败"
        exit 1
    fi
}

# 编译项目
build_project() {
    print_info "编译项目..."
    
    local jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    print_info "使用 $jobs 个并行任务"
    
    make -j$jobs
    
    if [ $? -eq 0 ]; then
        print_success "编译成功"
    else
        print_error "编译失败"
        exit 1
    fi
}

# 运行测试
run_tests() {
    if [ "$RUN_TESTS" = "yes" ]; then
        print_info "运行测试..."
        
        if [ -f "http_parser_test" ]; then
            print_info "运行HTTP解析器测试..."
            ./http_parser_test
        fi
        
        if [ -f "http_message_test" ]; then
            print_info "运行HTTP消息测试..."
            ./http_message_test
        fi
        
        if [ -f "chunked_test" ]; then
            print_info "运行Chunked编码测试..."
            ./chunked_test
        fi
        
        print_success "所有测试通过"
    fi
}

# 安装项目
install_project() {
    if [ "$INSTALL" = "yes" ]; then
        print_info "安装项目..."
        sudo make install
        print_success "安装完成"
    fi
}

# 显示帮助信息
show_help() {
    echo "StdHTTPS 构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help          显示此帮助信息"
    echo "  -t, --type TYPE     构建类型 (Debug|Release|RelWithDebInfo)"
    echo "  -j, --jobs N        并行任务数量"
    echo "  --test              运行测试"
    echo "  --install           安装到系统"
    echo "  --clean             只清理构建目录"
    echo ""
    echo "示例:"
    echo "  $0                  默认构建"
    echo "  $0 -t Debug         调试构建"
    echo "  $0 -t Release       发布构建"
    echo "  $0 --test           构建并测试"
    echo "  $0 --install        构建并安装"
}

# 清理构建
clean_build() {
    print_info "清理构建目录..."
    if [ -d "build" ]; then
        rm -rf build
        print_success "构建目录已清理"
    else
        print_info "构建目录不存在，无需清理"
    fi
}

# 主函数
main() {
    print_info "StdHTTPS 自动构建脚本"
    print_info "========================"
    
    # 解析命令行参数
    BUILD_TYPE="RelWithDebInfo"
    RUN_TESTS="no"
    INSTALL="no"
    CLEAN_ONLY="no"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -t|--type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            --test)
                RUN_TESTS="yes"
                shift
                ;;
            --install)
                INSTALL="yes"
                shift
                ;;
            --clean)
                CLEAN_ONLY="yes"
                shift
                ;;
            *)
                print_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    if [ "$CLEAN_ONLY" = "yes" ]; then
        clean_build
        exit 0
    fi
    
    # 检测系统
    detect_os
    
    # 检查依赖
    check_dependencies
    
    # 创建构建目录
    create_build_dir
    
    # 配置项目
    configure_project
    
    # 编译项目
    build_project
    
    # 运行测试
    run_tests
    
    # 安装项目
    install_project
    
    print_success "构建完成！"
    print_info "可执行文件位于: $(pwd)"
    print_info "运行示例:"
    print_info "  ./server_example    # 启动HTTP服务器示例"
    print_info "  ./client_example    # 运行HTTP客户端示例"
}

# 检查是否在项目根目录
if [ ! -f "CMakeLists.txt" ]; then
    print_error "请在项目根目录下运行此脚本"
    exit 1
fi

# 运行主函数
main "$@"