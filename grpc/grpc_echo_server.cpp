#include <iostream>
#include <memory>
#include <string>
 
#include <grpc++/grpc++.h>
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
 
#include "echoserver.grpc.pb.h"
 
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class EchoServiceImpl final : public EchoService::Service{
        //override echo 
        ::grpc::Status echo(::grpc::ServerContext* context, const ::EchoRequest* request, ::EchoResponse* response){
                std::string prefix("Hello ");
                response->set_response(prefix + request->request());
                return Status::OK;
        }
};
 
void RunServer(){
        std::string server_address("0.0.0.0:50051");
        EchoServiceImpl echoservice;

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&echoservice);
        std::unique_ptr<Server> server(builder.BuildAndStart());

        std::cout << "Server listening on " << server_address << std::endl;

        server->Wait();
}

 
int main(int argc, char *argv[]){
        RunServer();

        return 0x0;
}
