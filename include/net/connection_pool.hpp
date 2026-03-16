/**
 * @file connection_pool.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Reusable TCP connection pool with keep-alive and idle timeout
 * @version 1.1.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <optional>

#include "socket.hpp"
#include "socket_address.hpp"
#include "internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

/**
 * @brief Configuration for a connection pool
 */
struct PoolConfig {
	size_t   max_connections   = 16;      // Maximum total connections
	size_t   max_per_host      = 4;       // Maximum connections per host
	uint32_t idle_timeout_ms   = 30000;   // Idle connection timeout (30s)
	uint32_t connect_timeout_ms = 5000;   // Connection attempt timeout (5s)
	bool     enable_keepalive  = true;    // Enable TCP keep-alive
};

/**
 * @brief Reusable TCP connection pool
 * 
 * Manages a pool of TCP connections that can be checked out and returned.
 * Connections are keyed by SocketAddress and automatically recycled when
 * idle timeout expires.
 * 
 * @tparam T The IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class ConnectionPool {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>,
		"Invalid IP version.");

public:
	using address_type = SocketAddress<T>;
	using clock_type   = std::chrono::steady_clock;
	using time_point   = clock_type::time_point;

	/**
	 * @brief A pooled connection handle (RAII — returns to pool on destruction)
	 */
	class Handle {
	public:
		Handle() noexcept = default;

		Handle(Socket<T>&& sock, ConnectionPool* pool, const address_type& addr) noexcept
			: socket_(std::move(sock)), pool_(pool), addr_(addr), valid_(true) {}

		~Handle() noexcept {
			if (valid_ && pool_ && socket_.is_open()) {
				pool_->return_connection(std::move(socket_), addr_);
			}
		}

		// Non-copyable, movable
		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;

		Handle(Handle&& other) noexcept
			: socket_(std::move(other.socket_))
			, pool_(other.pool_)
			, addr_(other.addr_)
			, valid_(other.valid_) {
			other.valid_ = false;
			other.pool_ = nullptr;
		}

		Handle& operator=(Handle&& other) noexcept {
			if (this != &other) {
				if (valid_ && pool_ && socket_.is_open()) {
					pool_->return_connection(std::move(socket_), addr_);
				}
				socket_ = std::move(other.socket_);
				pool_ = other.pool_;
				addr_ = other.addr_;
				valid_ = other.valid_;
				other.valid_ = false;
				other.pool_ = nullptr;
			}
			return *this;
		}

		Socket<T>& socket() noexcept { return socket_; }
		const Socket<T>& socket() const noexcept { return socket_; }
		bool is_valid() const noexcept { return valid_ && socket_.is_open(); }

		/**
		 * @brief Detach the socket — it will NOT be returned to the pool
		 */
		Socket<T> detach() noexcept {
			valid_ = false;
			return std::move(socket_);
		}

		/**
		 * @brief Invalidate — socket will be closed, not returned to pool
		 */
		void invalidate() noexcept {
			valid_ = false;
			socket_.close();
		}

	private:
		Socket<T>       socket_;
		ConnectionPool* pool_  = nullptr;
		address_type    addr_;
		bool            valid_ = false;
	};

	// ─── Construction ───────────────────

	explicit ConnectionPool(PoolConfig config = {}) noexcept
		: config_(config) {}

	~ConnectionPool() noexcept { clear(); }

	// Non-copyable
	ConnectionPool(const ConnectionPool&) = delete;
	ConnectionPool& operator=(const ConnectionPool&) = delete;

	// ─── Connection Management ──────────

	/**
	 * @brief Acquire a connection to the given address
	 * 
	 * Returns a pooled idle connection if available, otherwise creates a new one.
	 * The Handle is RAII — the connection returns to the pool when the Handle
	 * is destroyed.
	 * 
	 * @param addr Target socket address
	 * @return Handle wrapping the socket, or error
	 */
	std::expected<Handle, core::Error> acquire(const address_type& addr) {
		// Try to reuse an idle connection
		evict_expired();
		auto reused = try_reuse(addr);
		if (reused.has_value()) {
			return Handle(std::move(*reused), this, addr);
		}

		// Check pool limits
		if (total_idle() >= config_.max_connections) {
			// Try evicting the oldest idle connection
			if (!evict_oldest()) {
				return std::unexpected(core::Error::PoolExhausted);
			}
		}

		// Create a new connection
		Socket<T> sock;
		if (auto err = sock.create(); core::is_error(err)) {
			return std::unexpected(err);
		}

		if (config_.connect_timeout_ms > 0) {
			sock.set_timeout(config_.connect_timeout_ms);
		}

		if (auto err = sock.connect(addr); core::is_error(err)) {
			return std::unexpected(err);
		}

		// Reset timeout after connect
		if (config_.connect_timeout_ms > 0) {
			sock.set_timeout(0);
		}

		return Handle(std::move(sock), this, addr);
	}

	/**
	 * @brief Close all idle connections in the pool
	 */
	void clear() noexcept {
		for (auto& entry : idle_) {
			entry.socket.close();
		}
		idle_.clear();
	}

	/**
	 * @brief Remove expired idle connections
	 */
	void evict_expired() noexcept {
		auto now = clock_type::now();
		auto timeout = std::chrono::milliseconds(config_.idle_timeout_ms);
		std::erase_if(idle_, [&](auto& entry) {
			if (now - entry.last_used > timeout) {
				entry.socket.close();
				return true;
			}
			return false;
		});
	}

	// ─── Queries ────────────────────────

	/**
	 * @brief Get number of idle connections in the pool
	 */
	size_t total_idle() const noexcept { return idle_.size(); }

	/**
	 * @brief Get number of idle connections for a specific address
	 */
	size_t idle_for(const address_type& addr) const noexcept {
		size_t count = 0;
		for (const auto& entry : idle_) {
			if (entry.addr == addr) ++count;
		}
		return count;
	}

	/**
	 * @brief Get pool configuration
	 */
	const PoolConfig& config() const noexcept { return config_; }

private:
	struct IdleEntry {
		Socket<T>    socket;
		address_type addr;
		time_point   last_used;
	};

	PoolConfig             config_;
	std::vector<IdleEntry> idle_;

	/**
	 * @brief Try to reuse an idle connection to the given address
	 */
	std::optional<Socket<T>> try_reuse(const address_type& addr) {
		for (auto it = idle_.begin(); it != idle_.end(); ++it) {
			if (it->addr == addr && it->socket.is_open()) {
				auto sock = std::move(it->socket);
				idle_.erase(it);
				return sock;
			}
		}
		return std::nullopt;
	}

	/**
	 * @brief Return a connection to the idle pool
	 */
	void return_connection(Socket<T>&& sock, const address_type& addr) {
		if (!sock.is_open()) return;

		// Check per-host limit
		if (idle_for(addr) >= config_.max_per_host) {
			sock.close();
			return;
		}

		// Check total limit
		if (idle_.size() >= config_.max_connections) {
			evict_oldest();
		}

		idle_.push_back({std::move(sock), addr, clock_type::now()});
	}

	/**
	 * @brief Evict the oldest idle connection
	 * @return true if a connection was evicted
	 */
	bool evict_oldest() noexcept {
		if (idle_.empty()) return false;
		auto oldest = std::min_element(idle_.begin(), idle_.end(),
			[](const auto& a, const auto& b) { return a.last_used < b.last_used; });
		oldest->socket.close();
		idle_.erase(oldest);
		return true;
	}
};

} // namespace net
} // namespace etherz
