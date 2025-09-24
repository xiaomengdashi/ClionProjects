// 格式化文件列表
#include <gtest/gtest.h>
#include "../include/ftp_protocol.h"
#include <experimental/filesystem>

using namespace FTP;

using namespace std::experimental::filesystem::v1;
#include <fstream>

namespace {
    // 测试夹具类
    class FormatFileListTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // 创建测试目录结构
            testDir = "test_dir";
            create_directory(testDir);
            
            // 创建测试文件
            std::ofstream(testDir + "/test1.txt") << "test content 1";
            chmod((testDir + "/test1.txt").c_str(), 0644);
            std::ofstream(testDir + "/test2.txt") << "test content 2";
            chmod((testDir + "/test2.txt").c_str(), 0644);
            
            // 创建子目录
            create_directory(testDir + "/subdir");
        }

        void TearDown() override {
            // 清理测试目录
            remove_all(testDir);
        }

        std::string testDir;
    };

    // 测试详细列表模式
    TEST_F(FormatFileListTest, DetailedListTest) {
        std::string result = FTP::Utils::formatFileList(testDir, true);
        std::cout << result << std::endl;
        
        // 验证输出包含预期的文件
        EXPECT_TRUE(result.find("test1.txt") != std::string::npos);
        EXPECT_TRUE(result.find("test2.txt") != std::string::npos);
        EXPECT_TRUE(result.find("subdir") != std::string::npos);
        
        // 验证详细信息的格式
        EXPECT_TRUE(result.find("-rw-r--r--") != std::string::npos); // 文件权限
        EXPECT_TRUE(result.find("d") != std::string::npos); // 目录标记
    }

    // 测试简单列表模式
    TEST_F(FormatFileListTest, SimpleListTest) {
        std::string result = FTP::Utils::formatFileList(testDir, false);
        std::cout << result << std::endl;

        // 验证输出只包含文件名
        EXPECT_TRUE(result.find("test1.txt\r\n") != std::string::npos);
        EXPECT_TRUE(result.find("test2.txt\r\n") != std::string::npos);
        EXPECT_TRUE(result.find("subdir\r\n") != std::string::npos);
        
        // 验证不包含详细信息
        EXPECT_FALSE(result.find("-rw-r--r--") != std::string::npos);
    }

    // 测试不存在的目录
    TEST_F(FormatFileListTest, NonExistentDirectoryTest) {
                std::string result = FTP::Utils::formatFileList("non_existent_dir");
        EXPECT_EQ(result, "550 Failed to open directory\r\n");
    }

    // 测试 parseCommand 函数
    class ParseCommandTest : public ::testing::Test {
    protected:
        // 可以添加 SetUp 和 TearDown 方法，如果需要的话
    };

    // 测试只有命令没有参数的情况
    TEST_F(ParseCommandTest, CommandOnly) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("QUIT\r\n");
        EXPECT_EQ(result.first, "QUIT");
        EXPECT_EQ(result.second, "");
    }

    // 测试命令和参数的情况
    TEST_F(ParseCommandTest, CommandWithParameter) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("USER anonymous\r\n");
        EXPECT_EQ(result.first, "USER");
        EXPECT_EQ(result.second, "anonymous");
    }

    // 测试参数包含空格的情况
    TEST_F(ParseCommandTest, ParameterWithSpaces) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("STOR my file with spaces.txt\r\n");
        EXPECT_EQ(result.first, "STOR");
        EXPECT_EQ(result.second, "my file with spaces.txt");
    }

    // 测试命令大小写不敏感
    TEST_F(ParseCommandTest, CaseInsensitiveCommand) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("user anonymous\r\n");
        EXPECT_EQ(result.first, "USER");
        EXPECT_EQ(result.second, "anonymous");
    }

    // 测试参数前后有空格的情况
    TEST_F(ParseCommandTest, ParameterTrimmed) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("  PORT 1,2,3,4,5,6  \r\n");
        EXPECT_EQ(result.first, "PORT");
        EXPECT_EQ(result.second, "1,2,3,4,5,6");
    }

    // 测试空字符串
    TEST_F(ParseCommandTest, EmptyString) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("\r\n");
        EXPECT_EQ(result.first, "");
        EXPECT_EQ(result.second, "");
    }

    // 测试只有空格的字符串
    TEST_F(ParseCommandTest, OnlySpaces) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("   \r\n");
        EXPECT_EQ(result.first, "");
        EXPECT_EQ(result.second, "");
    }

    // 测试没有CRLF的命令
    TEST_F(ParseCommandTest, NoCRLF) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("NOOP");
        EXPECT_EQ(result.first, "NOOP");
        EXPECT_EQ(result.second, "");
    }

    // 测试没有CRLF的命令和参数
    TEST_F(ParseCommandTest, NoCRLFWithParam) {
        std::pair<std::string, std::string> result = FTP::Utils::parseCommand("CWD /path/to/dir");
        EXPECT_EQ(result.first, "CWD");
        EXPECT_EQ(result.second, "/path/to/dir");
    }

}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}