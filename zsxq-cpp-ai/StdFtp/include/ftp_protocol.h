/*
 * FTP协议定义头文件
 * 包含FTP协议命令、响应码和协议相关的常量定义
 */
#ifndef FTP_PROTOCOL_H
#define FTP_PROTOCOL_H

#include <string>
#include <map>

namespace FTP {

// FTP响应码定义
enum ResponseCode {
    // 1xx - 预备状态
    RESTART_MARKER = 110,           // 重新启动标记回复
    SERVICE_READY_IN = 120,          // 服务在nnn分钟内准备好
    DATA_CONNECTION_ALREADY_OPEN = 125,  // 数据连接已打开，开始传输
    FILE_STATUS_OK = 150,            // 文件状态正常，准备打开数据连接

    // 2xx - 完成状态
    COMMAND_OK = 200,                // 命令成功
    COMMAND_NOT_IMPLEMENTED = 202,  // 命令未实现
    SYSTEM_STATUS = 211,             // 系统状态
    DIRECTORY_STATUS = 212,          // 目录状态
    FILE_STATUS = 213,               // 文件状态
    HELP_MESSAGE = 214,              // 帮助信息
    SYSTEM_TYPE = 215,               // 系统类型
    SERVICE_READY = 220,             // 服务就绪
    SERVICE_CLOSING = 221,           // 服务关闭控制连接
    DATA_CONNECTION_OPEN = 225,      // 数据连接打开，没有传输正在进行
    CLOSING_DATA_CONNECTION = 226,   // 关闭数据连接，请求的文件操作成功
    ENTERING_PASSIVE_MODE = 227,     // 进入被动模式
    ENTERING_EXTENDED_PASSIVE_MODE = 229, // 进入扩展被动模式
    USER_LOGGED_IN = 230,            // 用户已登录
    FILE_ACTION_OK = 250,            // 请求的文件操作完成
    PATHNAME_CREATED = 257,          // 路径名已创建

    // 3xx - 中间状态
    USER_NAME_OK = 331,              // 用户名正确，需要密码
    NEED_ACCOUNT = 332,              // 需要登录账户
    FILE_ACTION_PENDING = 350,       // 请求的文件操作等待进一步信息

    // 4xx - 暂时拒绝
    SERVICE_NOT_AVAILABLE = 421,     // 服务不可用
    CANT_OPEN_DATA_CONNECTION = 425, // 无法打开数据连接
    CONNECTION_CLOSED = 426,         // 连接关闭，传输中止
    FILE_ACTION_NOT_TAKEN = 450,     // 请求的文件操作未执行
    ACTION_ABORTED = 451,            // 请求的操作中止：本地错误
    INSUFFICIENT_STORAGE = 452,      // 请求的操作未执行：系统存储空间不足

    // 5xx - 永久拒绝
    SYNTAX_ERROR = 500,              // 语法错误，命令无法识别
    SYNTAX_ERROR_IN_PARAMETERS = 501, // 参数语法错误
    COMMAND_NOT_IMPLEMENTED_FOR_PARAMETER = 502, // 命令未实现
    BAD_SEQUENCE = 503,              // 错误的命令序列
    COMMAND_NOT_IMPLEMENTED_FOR_TYPE = 504, // 该参数的命令未实现
    NOT_LOGGED_IN = 530,             // 未登录
    NEED_ACCOUNT_FOR_STORING = 532,  // 存储文件需要账户
    FILE_UNAVAILABLE = 550,          // 请求的操作未执行：文件不可用
    PAGE_TYPE_UNKNOWN = 551,         // 请求的操作中止：页面类型未知
    EXCEEDED_STORAGE = 552,          // 请求的文件操作中止：超出存储分配
    FILE_NAME_NOT_ALLOWED = 553      // 请求的操作未执行：文件名不被允许
};

// FTP命令定义
class Commands {
public:
    // 访问控制命令
    static constexpr const char* USER = "USER";  // 用户名
    static constexpr const char* PASS = "PASS";  // 密码
    static constexpr const char* ACCT = "ACCT";  // 账户
    static constexpr const char* CWD = "CWD";    // 改变工作目录
    static constexpr const char* CDUP = "CDUP";  // 改变到父目录
    static constexpr const char* SMNT = "SMNT";  // 结构挂载
    static constexpr const char* REIN = "REIN";  // 重新初始化
    static constexpr const char* QUIT = "QUIT";  // 退出

    // 传输参数命令
    static constexpr const char* PORT = "PORT";  // 数据端口
    static constexpr const char* PASV = "PASV";  // 被动模式
    static constexpr const char* TYPE = "TYPE";  // 传输类型
    static constexpr const char* STRU = "STRU";  // 文件结构
    static constexpr const char* MODE = "MODE";  // 传输模式

    // FTP服务命令
    static constexpr const char* RETR = "RETR";  // 下载文件
    static constexpr const char* STOR = "STOR";  // 上传文件
    static constexpr const char* STOU = "STOU";  // 唯一存储
    static constexpr const char* APPE = "APPE";  // 追加
    static constexpr const char* ALLO = "ALLO";  // 分配
    static constexpr const char* REST = "REST";  // 重新开始
    static constexpr const char* RNFR = "RNFR";  // 重命名来源
    static constexpr const char* RNTO = "RNTO";  // 重命名目标
    static constexpr const char* ABOR = "ABOR";  // 中止
    static constexpr const char* DELE = "DELE";  // 删除文件
    static constexpr const char* RMD = "RMD";    // 删除目录
    static constexpr const char* MKD = "MKD";    // 创建目录
    static constexpr const char* PWD = "PWD";    // 打印工作目录
    static constexpr const char* LIST = "LIST";  // 列表
    static constexpr const char* NLST = "NLST";  // 名字列表
    static constexpr const char* SITE = "SITE";  // 站点参数
    static constexpr const char* SYST = "SYST";  // 系统
    static constexpr const char* STAT = "STAT";  // 状态
    static constexpr const char* HELP = "HELP";  // 帮助
    static constexpr const char* NOOP = "NOOP";  // 空操作

    // 扩展命令
    static constexpr const char* FEAT = "FEAT";  // 特性
    static constexpr const char* OPTS = "OPTS";  // 选项
    static constexpr const char* SIZE = "SIZE";  // 文件大小
    static constexpr const char* MDTM = "MDTM";  // 文件修改时间
    static constexpr const char* MLST = "MLST";  // 机器列表
    static constexpr const char* MLSD = "MLSD";  // 机器目录列表
};

// 传输类型
enum class TransferType {
    ASCII = 'A',   // ASCII类型
    EBCDIC = 'E',  // EBCDIC类型
    IMAGE = 'I',   // 二进制类型
    LOCAL = 'L'    // 本地类型
};

// 传输模式
enum class TransferMode {
    STREAM = 'S',      // 流模式
    BLOCK = 'B',       // 块模式
    COMPRESSED = 'C'   // 压缩模式
};

// 文件结构
enum class FileStructure {
    FILE = 'F',        // 文件结构
    RECORD = 'R',      // 记录结构
    PAGE = 'P'         // 页面结构
};

// FTP配置常量
struct Config {
    static constexpr int DEFAULT_CONTROL_PORT = 21;    // 默认控制端口
    static constexpr int DEFAULT_DATA_PORT = 20;       // 默认数据端口
    static constexpr int BUFFER_SIZE = 8192;           // 缓冲区大小
    static constexpr int MAX_COMMAND_LENGTH = 512;     // 最大命令长度
    static constexpr int MAX_PATH_LENGTH = 4096;       // 最大路径长度
    static constexpr int LISTEN_BACKLOG = 5;           // 监听队列长度
    static constexpr int SESSION_TIMEOUT = 300;        // 会话超时时间（秒）
};

// 工具函数
class Utils {
public:
    // 生成FTP响应消息
    static std::string formatResponse(int code, const std::string& message);
    
    // 解析FTP命令
    static std::pair<std::string, std::string> parseCommand(const std::string& line);
    
    // 格式化文件列表
    static std::string formatFileList(const std::string& path, bool detailed = true);
    
    // 解析PORT命令参数
    static bool parsePortCommand(const std::string& params, std::string& host, int& port);
    
    // 生成PASV响应
    static std::string generatePasvResponse(const std::string& host, int port);
};

} // namespace FTP

#endif // FTP_PROTOCOL_H