#include "test_framework.hpp"
#include "net/service_discovery.hpp"

namespace en = etherz::net;

// ─── ServiceInfo (v2.5.0) ─────────────

TEST_CASE(service_info_full_name) {
	en::ServiceInfo svc;
	svc.name = "My Web Server";
	svc.type = "_http._tcp.local";
	CHECK_EQ(svc.full_name(), std::string("My Web Server._http._tcp.local"));
}

TEST_CASE(service_info_defaults) {
	en::ServiceInfo svc;
	CHECK_EQ(svc.port, static_cast<uint16_t>(0));
	CHECK_TRUE(svc.name.empty());
	CHECK_TRUE(svc.txt.empty());
}

// ─── ServiceRegistrar ─────────────────

TEST_CASE(service_registrar_register) {
	en::ServiceRegistrar reg;
	CHECK_TRUE(reg.services().empty());

	en::ServiceInfo svc;
	svc.name = "Test Service";
	svc.type = "_test._tcp.local";
	svc.port = 8080;
	reg.register_service(svc);

	CHECK_EQ(reg.services().size(), static_cast<size_t>(1));
	CHECK_EQ(reg.services()[0].name, std::string("Test Service"));
	CHECK_EQ(reg.services()[0].port, static_cast<uint16_t>(8080));
}

TEST_CASE(service_registrar_unregister) {
	en::ServiceRegistrar reg;

	en::ServiceInfo svc1;
	svc1.name = "Service A";
	svc1.type = "_a._tcp.local";
	reg.register_service(svc1);

	en::ServiceInfo svc2;
	svc2.name = "Service B";
	svc2.type = "_b._tcp.local";
	reg.register_service(svc2);

	CHECK_EQ(reg.services().size(), static_cast<size_t>(2));

	reg.unregister_service("Service A");
	CHECK_EQ(reg.services().size(), static_cast<size_t>(1));
	CHECK_EQ(reg.services()[0].name, std::string("Service B"));
}

TEST_CASE(service_registrar_announce) {
	en::ServiceRegistrar reg;
	en::ServiceInfo svc;
	svc.name = "Hello";
	svc.type = "_http._tcp.local";
	svc.address = en::Ip<4>(192, 168, 1, 1);
	svc.port = 80;
	reg.register_service(svc);

	auto err = reg.announce();
	CHECK_TRUE(etherz::core::is_ok(err));
}
