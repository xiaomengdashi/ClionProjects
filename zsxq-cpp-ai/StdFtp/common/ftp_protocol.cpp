/*
 * FTP协议实现文件
 * 实现FTP协议相关的工具函数
 */
#include "../include/ftp_protocol.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <cstring>

namespace FTP {

// 生成FTP响应消息
std::string Utils::formatResponse(int code, const std::string& message) {
    std::ostringstream oss;
    oss << code << " " << message << "\r\n";
    return oss.str();
}

// 解析FTP命令
// 返回值: pair<命令, 参数>
std::pair<std::string, std::string> Utils::parseCommand(const std::string& line) {
    std::string trimmedLine = line;
    
    // 移除末尾的\r\n
    while (!trimmedLine.empty() && 
           (trimmedLine.back() == '\r' || trimmedLine.back() == '\n')) {
        trimmedLine.pop_back();
    }
    
    // 查找第一个非空格字符作为命令的开始
    size_t commandStart = trimmedLine.find_first_not_of(" \t");
    if (commandStart == std::string::npos) {
        return {"", ""}; // 空字符串或只有空格
    }

    // 查找第一个空格字符作为命令的结束，参数的开始
    size_t spacePos = trimmedLine.find(' ', commandStart);

    std::string command;
    std::string params;

    if (spacePos == std::string::npos) {
        // 没有参数的命令
        command = trimmedLine.substr(commandStart);
        params = "";
    } else {
        // 分离命令和参数
        command = trimmedLine.substr(commandStart, spacePos - commandStart);
        params = trimmedLine.substr(spacePos + 1);
    }

    // 命令转换为大写
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    // 去除参数前后的空格
    size_t start = params.find_first_not_of(" \t");
    if (start == std::string::npos) {
        params = "";
    } else {
        size_t end = params.find_last_not_of(" \t");
        if (end == std::string::npos) {
            params = params.substr(start);
        } else {
            params = params.substr(start, end - start + 1);
        }
    }
    
    return {command, params};
}

// 格式化文件列表（Unix风格）
std::string Utils::formatFileList(const std::string& path, bool detailed) {
    std::ostringstream result;
    DIR* dir = opendir(path.c_str());
    
    if (!dir) {
        return "550 Failed to open directory\r\n";
    }
    
    struct dirent* entry;
    struct stat fileStat;
    
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string fullPath = path + "/" + entry->d_name;
        
        if (stat(fullPath.c_str(), &fileStat) == 0) {
            if (detailed) {
                // 详细列表格式（类似 ls -l）
                // 文件类型和权限
                result << ((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
                result << ((fileStat.st_mode & S_IRUSR) ? "r" : "-");
                result << ((fileStat.st_mode & S_IWUSR) ? "w" : "-");
                result << ((fileStat.st_mode & S_IXUSR) ? "x" : "-");
                result << ((fileStat.st_mode & S_IRGRP) ? "r" : "-");
                result << ((fileStat.st_mode & S_IWGRP) ? "w" : "-");
                result << ((fileStat.st_mode & S_IXGRP) ? "x" : "-");
                result << ((fileStat.st_mode & S_IROTH) ? "r" : "-");
                result << ((fileStat.st_mode & S_IWOTH) ? "w" : "-");
                result << ((fileStat.st_mode & S_IXOTH) ? "x" : "-");
                
                // 链接数
                result << " " << std::setw(3) << fileStat.st_nlink;
                
                // 所有者和组
                struct passwd* pw = getpwuid(fileStat.st_uid);
                struct group* gr = getgrgid(fileStat.st_gid);
                result << " " << (pw ? pw->pw_name : std::to_string(fileStat.st_uid));
                result << " " << (gr ? gr->gr_name : std::to_string(fileStat.st_gid));
                
                // 文件大小
                result << " " << std::setw(8) << fileStat.st_size;
                
                // 修改时间
                char timeStr[80];
                struct tm* timeinfo = localtime(&fileStat.st_mtime);
                strftime(timeStr, sizeof(timeStr), "%b %d %H:%M", timeinfo);
                result << " " << timeStr;
                
                // 文件名
                result << " " << entry->d_name;
                result << "\r\n";
            } else {
                // 简单列表格式
                result << entry->d_name << "\r\n";
            }
        }
    }
    
    closedir(dir);
    return result.str();
}

// 解析PORT命令参数
// PORT命令格式: h1,h2,h3,h4,p1,p2
// IP地址: h1.h2.h3.h4
// 端口: p1*256 + p2
bool Utils::parsePortCommand(const std::string& params, std::string& host, int& port) {
    std::vector<int> values;
    std::istringstream iss(params);
    std::string token;
    
    // 解析逗号分隔的6个数字
    while (std::getline(iss, token, ',')) {
        try {
            int val = std::stoi(token);
            if (val < 0 || val > 255) {
                return false;
            }
            values.push_back(val);
        } catch (...) {
            return false;
        }
    }
    
    if (values.size() != 6) {
        return false;
    }
    
    // 构建IP地址
    std::ostringstream hostStream;
    hostStream << values[0] << "." << values[1] << "." 
               << values[2] << "." << values[3];
    host = hostStream.str();
    
    // 计算端口
    port = values[4] * 256 + values[5];
    
    return true;
}

// 生成PASV响应
// 格式: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
std::string Utils::generatePasvResponse(const std::string& host, int port) {
    std::vector<int> octets;
    std::istringstream iss(host);
    std::string token;
    
    // 解析IP地址
    while (std::getline(iss, token, '.')) {
        octets.push_back(std::stoi(token));
    }
    
    // 计算端口的高低字节
    int p1 = port / 256;
    int p2 = port % 256;
    
    // 生成响应
    std::ostringstream response;
    response << "227 Entering Passive Mode (";
    response << octets[0] << "," << octets[1] << ",";
    response << octets[2] << "," << octets[3] << ",";
    response << p1 << "," << p2 << ").\r\n";
    
    return response.str();
}

} // namespace FTP