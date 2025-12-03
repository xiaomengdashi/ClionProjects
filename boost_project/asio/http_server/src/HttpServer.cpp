#include "HttpServer.h"
#include <iostream>

HttpServer::HttpServer(short port, std::size_t io_context_count)
    : io_context_pool_(io_context_count),
      acceptor_(io_context_pool_.GetAcceptorContext(), tcp::endpoint(tcp::v4(), port)),
      port_(port) {
  
  std::cout << "HTTP Server started on port " << port 
            << " with " << io_context_pool_.GetPoolSize() << " I/O contexts" << std::endl;
  
  DoAccept();
}

HttpServer::~HttpServer() {
  Stop();
}

void HttpServer::DoAccept() {
  acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
          // 将新连接分配给下一个io_context
          auto& target_io_context = io_context_pool_.GetNextIoContext();
          // 获取socket的原生句柄
          auto native_handle = socket.release();
          
          // 在目标io_context中创建新socket并启动会话
          asio::post(target_io_context, [this, native_handle, &target_io_context]() {
            tcp::socket new_socket(target_io_context);
            boost::system::error_code assign_ec;
            new_socket.assign(tcp::v4(), native_handle, assign_ec);
            if (!assign_ec) {
              std::make_shared<HttpSession>(std::move(new_socket))->Start();
            }
            // 如果assign失败，socket句柄会被丢弃（操作系统会关闭）
          });
        }
        DoAccept();
      }
    );
}

void HttpServer::Run() {
  io_context_pool_.Run();
}

void HttpServer::Stop() {
  io_context_pool_.Stop();
}