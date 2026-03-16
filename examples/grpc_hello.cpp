/**
 * @file grpc_hello.cpp
 * @brief gRPC Hello World example — server and client
 * 
 * Usage:
 *   grpc_hello server     — Start gRPC server on port 50051
 *   grpc_hello client     — Connect and call SayHello
 */

#include "../include/protocol/grpc_channel.hpp"
#include "../include/protocol/grpc_server.hpp"
#include "../include/protocol/protobuf.hpp"
#include <print>
#include <string>

namespace ep = etherz::protocol;
namespace ec = etherz::core;

// Proto fields:
//   HelloRequest  { string name = 1; }
//   HelloResponse { string message = 1; }

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: grpc_hello <server|client>\n");
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "server") {
		ep::GrpcServer server;

		// Register the SayHello method
		server.register_unary("/greet.Greeter/SayHello",
			[](const ep::ProtoMessage& request, const ep::GrpcContext&)
				-> std::pair<ep::ProtoMessage, ep::GrpcStatus>
			{
				std::string name = request.get_string(1);
				if (name.empty()) name = "World";

				std::print("Received request: name={}\n", name);

				ep::ProtoMessage response;
				response.set_string(1, "Hello, " + name + "!");
				return {response, ep::GrpcStatus::Ok};
			});

		auto err = server.listen(50051);
		if (ec::is_error(err)) {
			std::print("Listen failed: {}\n", ec::error_message(err));
			return 1;
		}

		std::print("gRPC server listening on port 50051\n");
		server.serve();
	}
	else if (mode == "client") {
		ep::GrpcChannel channel;

		auto err = channel.connect("localhost", 50051);
		if (ec::is_error(err)) {
			std::print("Connect failed: {}\n", ec::error_message(err));
			return 1;
		}

		std::print("Connected to gRPC server\n");

		// Build request
		ep::ProtoMessage request;
		request.set_string(1, "Etherz");

		// Make RPC call
		auto result = channel.unary_call("/greet.Greeter/SayHello", request);
		if (!result) {
			std::print("RPC failed: {}\n", ec::error_message(result.error()));
			return 1;
		}

		auto& response = *result;
		std::print("Status: {}\n", ep::grpc_status_name(response.status));

		if (response.ok()) {
			auto msg = response.message();
			std::print("Response: {}\n", msg.get_string(1));
		}

		channel.close();
	}

	return 0;
}
