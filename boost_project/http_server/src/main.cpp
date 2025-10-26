#include "HttpServer.h"

#include <iostream>

#include <boost/asio.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main(int argc, char *argv[]) {
  try {

    asio::io_context io_context;
    HttpServer server(io_context, 9090);
    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}