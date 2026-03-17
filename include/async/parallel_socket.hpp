/**
 * @file parallel_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Multi-threaded TCP server acceptor with thread pool dispatch
 * @version 4.0.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <functional>
#include <atomic>

#include "thread_pool.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  Parallel Acceptor Configuration
// ═══════════════════════════════════════════════

struct ParallelConfig {
	size_t   pool_threads        = 0;     // 0 = hardware_concurrency
	size_t   max_connections     = 1024;  // Maximum concurrent connections
	uint32_t accept_timeout_ms   = 100;   // Accept timeout for poll-like behavior
	int      listen_backlog      = 128;
};

// ═══════════════════════════════════════════════
//  Parallel Acceptor
// ═══════════════════════════════════════════════

/**
 * @brief Multi-threaded TCP server that dispatches connections to a thread pool
 * 
 * Accepts incoming TCP connections on a listening socket and dispatches
 * each connection's handler to a ThreadPool for parallel processing.
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 * 
 * Usage:
 *   ParallelAcceptor<Ip<4>> server;
 *   server.start(8080, [](Socket<Ip<4>>&& client) {
 *       // Handle client in a worker thread
 *       std::array<uint8_t, 4096> buf{};
 *       int n = client.recv(buf);
 *       client.send(std::span(buf.data(), n));
 *       client.close();
 *   });
 *   
 *   // ... later
 *   server.stop();
 */
template <typename T>
class ParallelAcceptor {
	static_assert(std::is_same_v<T, net::Ip<4>> || std::is_same_v<T, net::Ip<6>>,
		"Invalid IP version.");

public:
	using ConnectionHandler = std::function<void(net::Socket<T>&&)>;
	using address_type = net::SocketAddress<T>;

	ParallelAcceptor() noexcept = default;
	~ParallelAcceptor() noexcept { stop(); }

	// Non-copyable
	ParallelAcceptor(const ParallelAcceptor&) = delete;
	ParallelAcceptor& operator=(const ParallelAcceptor&) = delete;

	// ─── Lifecycle ──────────────────────

	/**
	 * @brief Start accepting connections on the given port
	 * @param port Port to listen on
	 * @param handler Callback invoked in a worker thread for each connection
	 * @param config Parallel acceptor configuration
	 */
	core::Error start(uint16_t port, ConnectionHandler handler,
		ParallelConfig config = {})
	{
		config_ = config;
		handler_ = std::move(handler);

		// Create thread pool
		pool_ = std::make_unique<ThreadPool>(config_.pool_threads);

		// Setup listener
		auto bind_addr = address_type(T{}, port);
		if (auto err = listener_.create(); core::is_error(err)) return err;
		listener_.set_reuse_addr(true);
		listener_.set_timeout(config_.accept_timeout_ms);
		if (auto err = listener_.bind(bind_addr); core::is_error(err)) return err;
		if (auto err = listener_.listen(config_.listen_backlog);
			core::is_error(err)) return err;

		running_.store(true);

		// Start accept loop in its own thread
		accept_thread_ = std::thread([this] { accept_loop(); });

		return core::Error::None;
	}

	/**
	 * @brief Stop accepting and shut down the thread pool
	 */
	void stop() noexcept {
		if (!running_.exchange(false)) return;

		if (accept_thread_.joinable()) {
			accept_thread_.join();
		}

		if (pool_) {
			pool_->shutdown();
			pool_.reset();
		}

		listener_.close();
	}

	// ─── Queries ────────────────────────

	bool is_running() const noexcept { return running_.load(); }

	size_t active_connections() const noexcept {
		return active_count_.load();
	}

	size_t total_accepted() const noexcept {
		return total_accepted_.load();
	}

private:
	net::Socket<T>                  listener_;
	std::unique_ptr<ThreadPool>     pool_;
	ConnectionHandler               handler_;
	ParallelConfig                  config_;
	std::thread                     accept_thread_;
	std::atomic<bool>               running_{false};
	std::atomic<size_t>             active_count_{0};
	std::atomic<size_t>             total_accepted_{0};

	/**
	 * @brief Main accept loop running in its own thread
	 */
	void accept_loop() {
		while (running_.load()) {
			auto result = listener_.accept();
			if (!result.has_value()) {
				continue;  // Timeout or error — retry
			}

			// Check connection limit
			if (active_count_.load() >= config_.max_connections) {
				auto client = result->take_client();
				client.close();
				continue;
			}

			auto client = result->take_client();
			total_accepted_.fetch_add(1);
			active_count_.fetch_add(1);

			// Dispatch to thread pool
			pool_->submit([this, sock = std::move(client)]() mutable {
				if (handler_) {
					handler_(std::move(sock));
				}
				active_count_.fetch_sub(1);
			});
		}
	}
};

} // namespace async
} // namespace etherz
