#include <boost/asio.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "UrlDecode.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  explicit HttpSession(tcp::socket socket);
  ~HttpSession();
  void Start();

private:
  void ReadRequest();
  void HandleDownload(const std::string &path);
  void DownloadRealFile(const std::string &filePath);
  void DownloadVirtualFile(const size_t fileSize);
  void SendFileChunk(std::ifstream file, std::streamsize fileSize);
  void SendVirtualData(size_t sentBytes);
  void ReadBody();
  void SendResponse(const std::string &response);

  // 服务端优雅关闭示例
  void GracefulShutdown();

private:
  tcp::socket socket_;
  asio::streambuf request_buffer_;
  std::ofstream output_file_;
  std::filesystem::path temp_file_path_;
  size_t content_length_ = 0;
  size_t received_bytes_ = 0;
  size_t file_size_ = 0;
  std::string dummy_buffer_;
};