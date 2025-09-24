#!/bin/bash
# FTP服务器和客户端自动化测试脚本

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试配置
TEST_PORT=2121
TEST_DIR="/tmp/ftp_test"
TEST_USER="testuser"
TEST_PASS="testpass"
SERVER_PID=""

# 打印函数
print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# 清理函数
cleanup() {
    print_info "清理测试环境..."
    
    # 停止服务器
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null
        wait $SERVER_PID 2>/dev/null
        print_info "已停止FTP服务器"
    fi
    
    # 清理测试目录
    rm -rf $TEST_DIR
    rm -f test_*.txt test_*.bin
    
    print_info "清理完成"
}

# 设置陷阱，确保清理
trap cleanup EXIT

# 构建项目
build_project() {
    print_test "构建FTP服务器和客户端..."
    
    if [ -x "./build.sh" ]; then
        ./build.sh make
    else
        make clean
        make
    fi
    
    if [ ! -f "bin/ftp_server" ] || [ ! -f "bin/ftp_client" ]; then
        print_fail "构建失败"
        exit 1
    fi
    
    print_pass "构建成功"
}

# 准备测试环境
prepare_test() {
    print_test "准备测试环境..."
    
    # 创建测试目录
    mkdir -p $TEST_DIR
    mkdir -p $TEST_DIR/subdir
    
    # 创建测试文件
    echo "这是一个测试文件" > $TEST_DIR/test.txt
    echo "子目录中的文件" > $TEST_DIR/subdir/subfile.txt
    dd if=/dev/zero of=$TEST_DIR/binary.bin bs=1024 count=10 2>/dev/null
    
    # 创建本地测试文件
    echo "本地测试文件内容" > test_upload.txt
    dd if=/dev/zero of=test_binary.bin bs=1024 count=5 2>/dev/null
    
    print_pass "测试环境准备完成"
}

# 启动FTP服务器
start_server() {
    print_test "启动FTP服务器..."
    
    ./bin/ftp_server -p $TEST_PORT -d $TEST_DIR > /tmp/ftp_server.log 2>&1 &
    SERVER_PID=$!
    
    # 等待服务器启动
    sleep 2
    
    # 检查服务器是否运行
    if ! ps -p $SERVER_PID > /dev/null; then
        print_fail "服务器启动失败"
        cat /tmp/ftp_server.log
        exit 1
    fi
    
    print_pass "FTP服务器已启动 (PID: $SERVER_PID, 端口: $TEST_PORT)"
}

# 创建FTP客户端命令文件
create_ftp_commands() {
    local cmd_file=$1
    shift
    echo "$@" | tr '|' '\n' > $cmd_file
}

# 执行FTP客户端测试
run_ftp_test() {
    local test_name=$1
    local commands=$2
    local expect_success=${3:-true}
    
    print_test "测试: $test_name"
    
    # 创建命令文件
    create_ftp_commands /tmp/ftp_commands.txt "$commands"
    
    # 执行客户端命令
    timeout 10 ./bin/ftp_client < /tmp/ftp_commands.txt > /tmp/ftp_output.txt 2>&1
    local result=$?
    
    if [ "$expect_success" = true ] && [ $result -eq 0 ]; then
        print_pass "$test_name 成功"
        return 0
    elif [ "$expect_success" = false ] && [ $result -ne 0 ]; then
        print_pass "$test_name 按预期失败"
        return 0
    else
        print_fail "$test_name 失败"
        echo "输出内容:"
        cat /tmp/ftp_output.txt
        return 1
    fi
}

# 测试套件
run_tests() {
    local total_tests=0
    local passed_tests=0
    
    print_info "开始运行测试套件..."
    echo ""
    
    # 测试1: 连接和登录
    ((total_tests++))
    if run_ftp_test "连接和登录" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|quit"; then
        ((passed_tests++))
    fi
    
    # 测试2: 目录操作
    ((total_tests++))
    if run_ftp_test "目录操作" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|pwd|ls|cd subdir|pwd|cd ..|quit"; then
        ((passed_tests++))
    fi
    
    # 测试3: 创建和删除目录
    ((total_tests++))
    if run_ftp_test "创建和删除目录" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|mkdir newdir|ls|rmdir newdir|ls|quit"; then
        ((passed_tests++))
    fi
    
    # 测试4: 文件上传（ASCII模式）
    ((total_tests++))
    if run_ftp_test "文件上传(ASCII)" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|ascii|put test_upload.txt|ls|quit"; then
        ((passed_tests++))
        # 验证文件是否上传
        if [ -f "$TEST_DIR/test_upload.txt" ]; then
            print_pass "文件上传验证成功"
        else
            print_fail "文件上传验证失败"
        fi
    fi
    
    # 测试5: 文件上传（二进制模式）
    ((total_tests++))
    if run_ftp_test "文件上传(Binary)" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|binary|put test_binary.bin|ls|quit"; then
        ((passed_tests++))
        # 验证文件是否上传
        if [ -f "$TEST_DIR/test_binary.bin" ]; then
            print_pass "二进制文件上传验证成功"
        else
            print_fail "二进制文件上传验证失败"
        fi
    fi
    
    # 测试6: 文件下载
    ((total_tests++))
    rm -f test_download.txt
    if run_ftp_test "文件下载" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|get test.txt test_download.txt|quit"; then
        ((passed_tests++))
        # 验证文件是否下载
        if [ -f "test_download.txt" ]; then
            print_pass "文件下载验证成功"
            # 验证内容
            if grep -q "这是一个测试文件" test_download.txt; then
                print_pass "文件内容验证成功"
            else
                print_fail "文件内容验证失败"
            fi
        else
            print_fail "文件下载验证失败"
        fi
    fi
    
    # 测试7: 删除文件
    ((total_tests++))
    if run_ftp_test "删除文件" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|delete test_upload.txt|ls|quit"; then
        ((passed_tests++))
        # 验证文件是否被删除
        if [ ! -f "$TEST_DIR/test_upload.txt" ]; then
            print_pass "文件删除验证成功"
        else
            print_fail "文件删除验证失败"
        fi
    fi
    
    # 测试8: 被动模式
    ((total_tests++))
    if run_ftp_test "被动模式传输" \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|ls|quit"; then
        ((passed_tests++))
    fi
    
    echo ""
    echo "======================================"
    echo "测试结果汇总"
    echo "======================================"
    echo -e "总测试数: ${total_tests}"
    echo -e "通过测试: ${GREEN}${passed_tests}${NC}"
    echo -e "失败测试: ${RED}$((total_tests - passed_tests))${NC}"
    
    if [ $passed_tests -eq $total_tests ]; then
        echo -e "${GREEN}所有测试通过！${NC}"
        return 0
    else
        echo -e "${RED}部分测试失败${NC}"
        return 1
    fi
}

# 性能测试
performance_test() {
    print_info "运行性能测试..."
    
    # 创建大文件
    print_test "创建100MB测试文件..."
    dd if=/dev/zero of=test_large.bin bs=1M count=100 2>/dev/null
    
    # 测试上传速度
    print_test "测试大文件上传..."
    start_time=$(date +%s)
    
    create_ftp_commands /tmp/ftp_perf.txt \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|binary|put test_large.bin|quit"
    
    timeout 60 ./bin/ftp_client < /tmp/ftp_perf.txt > /tmp/ftp_perf_output.txt 2>&1
    
    end_time=$(date +%s)
    upload_time=$((end_time - start_time))
    
    if [ -f "$TEST_DIR/test_large.bin" ]; then
        print_pass "上传成功，用时: ${upload_time}秒"
        upload_speed=$(echo "scale=2; 100 / $upload_time" | bc)
        print_info "上传速度: ${upload_speed} MB/s"
    else
        print_fail "上传失败"
    fi
    
    # 测试下载速度
    print_test "测试大文件下载..."
    rm -f test_download_large.bin
    start_time=$(date +%s)
    
    create_ftp_commands /tmp/ftp_perf2.txt \
        "open localhost $TEST_PORT|login $TEST_USER|$TEST_PASS|binary|get test_large.bin test_download_large.bin|quit"
    
    timeout 60 ./bin/ftp_client < /tmp/ftp_perf2.txt > /tmp/ftp_perf_output2.txt 2>&1
    
    end_time=$(date +%s)
    download_time=$((end_time - start_time))
    
    if [ -f "test_download_large.bin" ]; then
        print_pass "下载成功，用时: ${download_time}秒"
        download_speed=$(echo "scale=2; 100 / $download_time" | bc)
        print_info "下载速度: ${download_speed} MB/s"
    else
        print_fail "下载失败"
    fi
    
    # 清理大文件
    rm -f test_large.bin test_download_large.bin
}

# 并发测试
concurrency_test() {
    print_info "运行并发连接测试..."
    
    local num_clients=5
    local pids=()
    
    print_test "启动 $num_clients 个并发客户端..."
    
    for i in $(seq 1 $num_clients); do
        (
            create_ftp_commands /tmp/ftp_concurrent_$i.txt \
                "open localhost $TEST_PORT|login user$i|pass$i|pwd|ls|quit"
            
            ./bin/ftp_client < /tmp/ftp_concurrent_$i.txt > /tmp/ftp_concurrent_output_$i.txt 2>&1
        ) &
        pids+=($!)
    done
    
    # 等待所有客户端完成
    local all_success=true
    for pid in ${pids[@]}; do
        wait $pid
        if [ $? -ne 0 ]; then
            all_success=false
        fi
    done
    
    if [ "$all_success" = true ]; then
        print_pass "并发测试成功，服务器处理了 $num_clients 个并发连接"
    else
        print_fail "并发测试失败"
    fi
}

# 主函数
main() {
    echo "======================================"
    echo "FTP服务器和客户端自动化测试"
    echo "======================================"
    echo ""
    
    # 构建项目
    build_project
    
    # 准备测试环境
    prepare_test
    
    # 启动服务器
    start_server
    
    # 运行功能测试
    run_tests
    test_result=$?
    
    # 可选：运行性能测试
    if [ "$1" == "--performance" ]; then
        echo ""
        performance_test
    fi
    
    # 可选：运行并发测试
    if [ "$1" == "--concurrent" ]; then
        echo ""
        concurrency_test
    fi
    
    echo ""
    echo "======================================"
    if [ $test_result -eq 0 ]; then
        echo -e "${GREEN}测试完成 - 所有测试通过${NC}"
    else
        echo -e "${RED}测试完成 - 存在失败的测试${NC}"
    fi
    echo "======================================"
    
    exit $test_result
}

# 运行主函数
main "$@"