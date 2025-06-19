#include <iostream>
#include <memory>
#include <string>
#include <cassert>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include <grpc/support/port_platform.h>

#include "echoserver.grpc.pb.h"
 
using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
 
class EchoClient {
 public:
  explicit EchoClient(std::shared_ptr<Channel> channel)
      : stub_(EchoService::NewStub(channel)) {}
 
  std::string echo(const std::string& msg) {
    EchoRequest request;
    request.set_request(msg);
 
    EchoResponse reply;
    ClientContext context;
 
    CompletionQueue cq;
 
    Status status;
 
    std::unique_ptr<ClientAsyncResponseReader<EchoResponse> > rpc(
        stub_->Asyncecho(&context, request, &cq));
 
    rpc->Finish(&reply, &status, (void*)1);
    void* got_tag;
    bool ok = false;
  
    // 使用标准库 assert 替代 GPR_ASSERT
    assert(cq.Next(&got_tag, &ok));
  
    assert(got_tag == (void*)1);
  
    assert(ok);
 
    if (status.ok()) {
      return reply.response();
    } else {
      return "RPC failed";
    }
  }
 
 private:
  std::unique_ptr<EchoService::Stub> stub_;
};

int main(int argc, char *argv[]) {
  EchoClient client(grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials()));
  std::string msg("world");
  std::string reply = client.echo(msg);  // The actual RPC call!
  std::cout << "client received: " << reply << std::endl;
 
  return 0;
}