#include "net/internet_protocol.hpp"

namespace etn = etherz::net;

int main(int argc, char* argv[]) {

	auto a1 = etn::Ip(192, 168, 1, 50);
	auto a2 = etn::Ip<4>{"192.168.1.50"};
	a1.display();
	a2.display();

	return 0;
}