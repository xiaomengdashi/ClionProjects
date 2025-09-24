#!/bin/bash
# FTP服务器和客户端构建脚本

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "FTP服务器和客户端构建脚本"
    echo "用法: ./build.sh [选项]"
    echo ""
    echo "选项:"
    echo "  cmake    - 使用CMake构建"
    echo "  make     - 使用Make构建"
    echo "  clean    - 清理构建文件"
    echo "  test     - 运行测试"
    echo "  help     - 显示帮助信息"
    echo ""
    echo "如果不指定选项，默认使用Make构建"
}

# 使用CMake构建
build_with_cmake() {
    print_info "使用CMake构建..."
    
    # 创建构建目录
    mkdir -p build
    cd build
    
    # 配置CMake
    print_info "配置CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    
    if [ $? -ne 0 ]; then
        print_error "CMake配置失败"
        exit 1
    fi
    
    # 编译
    print_info "编译中..."
    make -j$(nproc)
    
    if [ $? -ne 0 ]; then
        print_error "编译失败"
        exit 1
    fi
    
    print_info "构建完成！"
    print_info "可执行文件位于: build/bin/"
    
    cd ..
}

# 使用Make构建
build_with_make() {
    print_info "使用Make构建..."
    
    # 执行make
    make release
    
    if [ $? -ne 0 ]; then
        print_error "Make构建失败"
        exit 1
    fi
    
    print_info "构建完成！"
    print_info "可执行文件位于: bin/"
}

# 清理构建文件
clean_build() {
    print_info "清理构建文件..."
    
    # 清理CMake构建
    if [ -d "build" ]; then
        rm -rf build
        print_info "已清理CMake构建文件"
    fi
    
    # 清理Make构建
    make clean 2>/dev/null
    print_info "已清理Make构建文件"
    
    print_info "清理完成！"
}

# 运行基本测试
run_tests() {
    print_info "运行基本测试..."
    
    # 检查服务器是否构建
    if [ ! -f "bin/ftp_server" ]; then
        print_warning "服务器未构建，先进行构建..."
        build_with_make
    fi
    
    # 检查客户端是否构建
    if [ ! -f "bin/ftp_client" ]; then
        print_warning "客户端未构建，先进行构建..."
        build_with_make
    fi
    
    # 创建测试目录
    print_info "创建测试环境..."
    mkdir -p /tmp/ftp/test
    echo "测试文件内容" > /tmp/ftp/test/test.txt
    
    # 启动服务器（后台）
    print_info "启动FTP服务器（端口2121）..."
    ./bin/ftp_server -p 2121 -d /tmp/ftp &
    SERVER_PID=$!
    
    # 等待服务器启动
    sleep 2
    
    # 检查服务器是否运行
    if ! ps -p $SERVER_PID > /dev/null; then
        print_error "服务器启动失败"
        exit 1
    fi
    
    print_info "FTP服务器已启动（PID: $SERVER_PID）"
    print_info "测试完成后，使用 'kill $SERVER_PID' 停止服务器"
    
    # 创建测试脚本
    cat > /tmp/ftp_test.txt << EOF
open localhost 2121
login testuser
testpass
pwd
ls
quit
EOF
    
    print_info "可以使用以下命令测试客户端："
    echo "./bin/ftp_client"
    echo ""
    print_info "测试命令序列："
    echo "  open localhost 2121"
    echo "  login testuser"
    echo "  [输入密码: testpass]"
    echo "  pwd"
    echo "  ls"
    echo "  quit"
}

# 主函数
main() {
    # 检查参数
    if [ $# -eq 0 ]; then
        # 默认使用Make构建
        build_with_make
        exit 0
    fi
    
    # 处理命令行参数
    case "$1" in
        cmake)
            build_with_cmake
            ;;
        make)
            build_with_make
            ;;
        clean)
            clean_build
            ;;
        test)
            run_tests
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"