#include "test_framework.hpp"
#include "net/socket.hpp"
#include "async/event_loop.hpp"
#include "async/io_backend.hpp"

namespace ea = etherz::async;

// ─── IoBackendKind Tests (v1.4.0) ──────

TEST_CASE(io_backend_default_windows) {
	// On Windows, default should be IOCP
	auto kind = ea::default_backend();
	CHECK_EQ(kind, ea::IoBackendKind::Iocp);
}

TEST_CASE(io_backend_name_poll) {
	CHECK_EQ(ea::backend_name(ea::IoBackendKind::Poll),
		std::string_view("poll"));
}

TEST_CASE(io_backend_name_iocp) {
	CHECK_EQ(ea::backend_name(ea::IoBackendKind::Iocp),
		std::string_view("iocp"));
}

TEST_CASE(io_backend_name_io_uring) {
	CHECK_EQ(ea::backend_name(ea::IoBackendKind::IoUring),
		std::string_view("io_uring"));
}

TEST_CASE(io_backend_name_kqueue) {
	CHECK_EQ(ea::backend_name(ea::IoBackendKind::Kqueue),
		std::string_view("kqueue"));
}

TEST_CASE(poll_backend_construction) {
	ea::PollBackend backend;
	CHECK_EQ(backend.size(), static_cast<size_t>(0));
	CHECK_TRUE(backend.empty());
}
