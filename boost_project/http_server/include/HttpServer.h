#include <boost/asio.hpp>

#include "HttpSession.h"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpServer {
public:
  HttpServer(asio::io_context &ioContext, short port);

private:
  void DoAccept();

private:
  tcp::acceptor acceptor_;
};