// SyncClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <boost/asio.hpp>
#include <thread>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
const int MAX_LENGTH = 1024 * 2;
const int HEAD_LENGTH = 2;
int main()
{
	try {
		//创建上下文服务
		io_context   ioc;
		//构造endpoint
		std::string raw_ip_address = "127.0.0.1";
		boost::system::error_code ec;
		address ip_address = make_address(raw_ip_address, ec);
		tcp::endpoint  remote_ep(ip_address, 10086);
		tcp::socket  sock(ioc);
		boost::system::error_code   error = error::host_not_found; ;
		sock.connect(remote_ep, error);
		if (error) {
			cout << "connect failed, code is " << error.value() << " error msg is " << error.message();
			return 0;
		}

		thread send_thread([&sock] {
			for (;;) {
				this_thread::sleep_for(std::chrono::milliseconds(2));
				const char* request = "hello world!";
				short request_length = strlen(request);
				char send_data[MAX_LENGTH] = { 0 };
				//转为网络字节序
				short request_host_length = boost::asio::detail::socket_ops::host_to_network_short(request_length);
				memcpy(send_data, &request_host_length, 2);
				memcpy(send_data + 2, request, request_length);
				write(sock, buffer(send_data, request_length + 2));
			}
			});

		thread recv_thread([&sock] {
			for (;;) {
				this_thread::sleep_for(std::chrono::milliseconds(2));
				cout << "begin to receive..." << endl;
				char reply_head[HEAD_LENGTH];
				size_t reply_length = read(sock, buffer(reply_head, HEAD_LENGTH));
				short msglen = 0;
				memcpy(&msglen, reply_head, HEAD_LENGTH);
				//转为本地字节序
				msglen = boost::asio::detail::socket_ops::network_to_host_short(msglen);
				char msg[MAX_LENGTH] = { 0 };
				size_t  msg_length = read(sock, buffer(msg, msglen));

				std::cout << "Reply is: ";
				std::cout.write(msg, msglen) << endl;
				std::cout << "Reply len is " << msglen;
				std::cout << "\n";
			}
			});

		send_thread.join();
		recv_thread.join();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << endl;
	}
	return 0;
}


