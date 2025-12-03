#include "HttpServer.h"

#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
  try {
    short port = 9090;
    std::size_t io_context_count = 0; // 0表示使用CPU核心数
    
    // 可以从命令行参数读取端口和io_context数量
    if (argc > 1) {
      port = static_cast<short>(std::stoi(argv[1]));
    }
    if (argc > 2) {
      io_context_count = std::stoul(argv[2]);
    }
    
    HttpServer server(port, io_context_count);
    server.Run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}