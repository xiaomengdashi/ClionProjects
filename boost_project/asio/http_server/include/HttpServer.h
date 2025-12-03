#include <boost/asio.hpp>
#include <memory>

#include "HttpSession.h"
#include "IoContextPool.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpServer {
public:
  HttpServer(short port, std::size_t io_context_count = 0);
  ~HttpServer();
  void Run();
  void Stop();

private:
  void DoAccept();

private:
  IoContextPool io_context_pool_;
  tcp::acceptor acceptor_;
  short port_;
};