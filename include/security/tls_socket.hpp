/**
 * @file tls_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief TLS-encrypted socket wrapper using Windows SChannel
 * @version 0.5.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <array>
#include <print>
#include <type_traits>

#include "tls_context.hpp"
#include "certificate.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#define SECURITY_WIN32
	#include <windows.h>
	#include <security.h>
	#include <schnlsp.h>
	#include <sspi.h>
	#pragma comment(lib, "secur32.lib")
	#pragma comment(lib, "crypt32.lib")
#endif

namespace etherz {
namespace security {

namespace impl {

#ifdef _WIN32
	/**
	 * @brief SSPI credentials handle guard (RAII)
	 */
	struct CredentialGuard {
		CredHandle handle{};
		bool acquired = false;

		~CredentialGuard() noexcept {
			if (acquired) FreeCredentialsHandle(&handle);
		}

		CredentialGuard() noexcept = default;
		CredentialGuard(const CredentialGuard&) = delete;
		CredentialGuard& operator=(const CredentialGuard&) = delete;

		CredentialGuard(CredentialGuard&& other) noexcept
			: handle(other.handle), acquired(other.acquired) {
			other.acquired = false;
		}
		CredentialGuard& operator=(CredentialGuard&& other) noexcept {
			if (this != &other) {
				if (acquired) FreeCredentialsHandle(&handle);
				handle = other.handle;
				acquired = other.acquired;
				other.acquired = false;
			}
			return *this;
		}
	};

	/**
	 * @brief SSPI security context guard (RAII)
	 */
	struct ContextGuard {
		CtxtHandle handle{};
		bool initialized = false;

		~ContextGuard() noexcept {
			if (initialized) DeleteSecurityContext(&handle);
		}

		ContextGuard() noexcept = default;
		ContextGuard(const ContextGuard&) = delete;
		ContextGuard& operator=(const ContextGuard&) = delete;

		ContextGuard(ContextGuard&& other) noexcept
			: handle(other.handle), initialized(other.initialized) {
			other.initialized = false;
		}
		ContextGuard& operator=(ContextGuard&& other) noexcept {
			if (this != &other) {
				if (initialized) DeleteSecurityContext(&handle);
				handle = other.handle;
				initialized = other.initialized;
				other.initialized = false;
			}
			return *this;
		}
	};

	/**
	 * @brief Acquire SChannel credentials
	 */
	inline core::Error acquire_credentials(CredentialGuard& cred, TlsRole role) noexcept {
		SCHANNEL_CRED schannel_cred{};
		schannel_cred.dwVersion = SCHANNEL_CRED_VERSION;
		schannel_cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;
		schannel_cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

		TimeStamp lifetime;
		SECURITY_STATUS status = AcquireCredentialsHandleW(
			nullptr,
			const_cast<SEC_WCHAR*>(UNISP_NAME_W),
			(role == TlsRole::Client) ? SECPKG_CRED_OUTBOUND : SECPKG_CRED_INBOUND,
			nullptr,
			&schannel_cred,
			nullptr,
			nullptr,
			&cred.handle,
			&lifetime
		);

		if (status != SEC_E_OK) return core::Error::HandshakeFailed;
		cred.acquired = true;
		return core::Error::None;
	}
#endif

} // namespace impl

/**
 * @brief TLS-encrypted socket wrapper
 * 
 * Wraps a Socket<T> with TLS encryption using Windows SChannel.
 * Provides the same send/recv interface but with transparent encryption.
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class TlsSocket {
	static_assert(std::is_same_v<T, net::Ip<4>> || std::is_same_v<T, net::Ip<6>>,
		"Invalid IP version.");

public:
	using protocol_type = T;
	using address_type  = net::SocketAddress<T>;
	using socket_type   = net::Socket<T>;

	TlsSocket() noexcept = default;
	~TlsSocket() noexcept { close(); }

	// Non-copyable, movable
	TlsSocket(const TlsSocket&) = delete;
	TlsSocket& operator=(const TlsSocket&) = delete;

	TlsSocket(TlsSocket&& other) noexcept
		: socket_(std::move(other.socket_))
		, context_(std::move(other.context_))
		, handshake_done_(other.handshake_done_)
#ifdef _WIN32
		, cred_(std::move(other.cred_))
		, sec_ctx_(std::move(other.sec_ctx_))
		, stream_sizes_(other.stream_sizes_)
#endif
	{
		other.handshake_done_ = false;
	}

	TlsSocket& operator=(TlsSocket&& other) noexcept {
		if (this != &other) {
			close();
			socket_ = std::move(other.socket_);
			context_ = std::move(other.context_);
			handshake_done_ = other.handshake_done_;
#ifdef _WIN32
			cred_ = std::move(other.cred_);
			sec_ctx_ = std::move(other.sec_ctx_);
			stream_sizes_ = other.stream_sizes_;
#endif
			other.handshake_done_ = false;
		}
		return *this;
	}

	/**
	 * @brief Create the underlying socket with TLS context
	 */
	core::Error create(const TlsContext& ctx) noexcept {
		context_ = ctx;
		auto err = socket_.create();
		if (core::is_error(err)) return err;

#ifdef _WIN32
		err = impl::acquire_credentials(cred_, context_.role());
		if (core::is_error(err)) return err;
#endif

		return core::Error::None;
	}

	/**
	 * @brief Connect and perform TLS handshake
	 */
	core::Error connect(const address_type& addr) noexcept {
		auto err = socket_.connect(addr);
		if (core::is_error(err)) return err;
		return perform_handshake();
	}

	/**
	 * @brief Send data over TLS connection
	 * @return Number of plaintext bytes sent, or -1 on error
	 */
	int send(std::span<const uint8_t> data) noexcept {
		if (!handshake_done_) return -1;

#ifdef _WIN32
		// Build encrypted message using stream sizes
		size_t msg_size = stream_sizes_.cbHeader
		                + data.size()
		                + stream_sizes_.cbTrailer;
		std::vector<uint8_t> msg_buf(msg_size, 0);

		// Copy header space + plaintext + trailer space
		std::copy(data.begin(), data.end(),
			msg_buf.begin() + stream_sizes_.cbHeader);

		SecBuffer buffers[4]{};
		buffers[0].pvBuffer   = msg_buf.data();
		buffers[0].cbBuffer   = stream_sizes_.cbHeader;
		buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

		buffers[1].pvBuffer   = msg_buf.data() + stream_sizes_.cbHeader;
		buffers[1].cbBuffer   = static_cast<unsigned long>(data.size());
		buffers[1].BufferType = SECBUFFER_DATA;

		buffers[2].pvBuffer   = msg_buf.data() + stream_sizes_.cbHeader + data.size();
		buffers[2].cbBuffer   = stream_sizes_.cbTrailer;
		buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

		buffers[3].BufferType = SECBUFFER_EMPTY;

		SecBufferDesc buf_desc;
		buf_desc.ulVersion = SECBUFFER_VERSION;
		buf_desc.cBuffers = 4;
		buf_desc.pBuffers = buffers;

		SECURITY_STATUS status = EncryptMessage(&sec_ctx_.handle, 0, &buf_desc, 0);
		if (status != SEC_E_OK) return -1;

		// Send encrypted data
		size_t total = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
		auto send_span = std::span<const uint8_t>(msg_buf.data(), total);
		int sent = socket_.send(send_span);
		return (sent > 0) ? static_cast<int>(data.size()) : -1;
#else
		// POSIX stub — no TLS, passthrough
		return socket_.send(data);
#endif
	}

	/**
	 * @brief Receive data over TLS connection
	 * @return Number of plaintext bytes received, or -1 on error
	 */
	int recv(std::span<uint8_t> buffer) noexcept {
		if (!handshake_done_) return -1;

#ifdef _WIN32
		// Receive encrypted data
		std::array<uint8_t, 16384> enc_buf{};
		int received = socket_.recv(enc_buf);
		if (received <= 0) return received;

		SecBuffer buffers[4]{};
		buffers[0].pvBuffer   = enc_buf.data();
		buffers[0].cbBuffer   = static_cast<unsigned long>(received);
		buffers[0].BufferType = SECBUFFER_DATA;
		buffers[1].BufferType = SECBUFFER_EMPTY;
		buffers[2].BufferType = SECBUFFER_EMPTY;
		buffers[3].BufferType = SECBUFFER_EMPTY;

		SecBufferDesc buf_desc;
		buf_desc.ulVersion = SECBUFFER_VERSION;
		buf_desc.cBuffers = 4;
		buf_desc.pBuffers = buffers;

		SECURITY_STATUS status = DecryptMessage(&sec_ctx_.handle, &buf_desc, 0, nullptr);
		if (status != SEC_E_OK) return -1;

		// Find decrypted data buffer
		for (int i = 0; i < 4; ++i) {
			if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].cbBuffer > 0) {
				size_t copy_len = (buffers[i].cbBuffer < buffer.size())
					? buffers[i].cbBuffer : buffer.size();
				std::copy_n(static_cast<uint8_t*>(buffers[i].pvBuffer),
					copy_len, buffer.data());
				return static_cast<int>(copy_len);
			}
		}
		return -1;
#else
		return socket_.recv(buffer);
#endif
	}

	/**
	 * @brief Shutdown TLS and close socket
	 */
	void close() noexcept {
		if (handshake_done_) {
#ifdef _WIN32
			// Send TLS shutdown notification
			DWORD shutdown_token = SCHANNEL_SHUTDOWN;
			SecBuffer out_buf;
			out_buf.pvBuffer   = &shutdown_token;
			out_buf.cbBuffer   = sizeof(shutdown_token);
			out_buf.BufferType = SECBUFFER_TOKEN;

			SecBufferDesc out_desc;
			out_desc.ulVersion = SECBUFFER_VERSION;
			out_desc.cBuffers = 1;
			out_desc.pBuffers = &out_buf;

			ApplyControlToken(&sec_ctx_.handle, &out_desc);
#endif
			handshake_done_ = false;
		}
		socket_.close();
	}

	// ─── State queries ──────────────────

	bool is_open() const noexcept { return socket_.is_open(); }
	bool handshake_complete() const noexcept { return handshake_done_; }
	net::impl::socket_t native_handle() const noexcept { return socket_.native_handle(); }

	/**
	 * @brief Get the TLS context
	 */
	const TlsContext& context() const noexcept { return context_; }

	/**
	 * @brief Get the underlying socket
	 */
	socket_type& socket() noexcept { return socket_; }
	const socket_type& socket() const noexcept { return socket_; }

private:
	socket_type socket_;
	TlsContext  context_;
	bool        handshake_done_ = false;

#ifdef _WIN32
	impl::CredentialGuard cred_;
	impl::ContextGuard    sec_ctx_;
	SecPkgContext_StreamSizes stream_sizes_{};

	/**
	 * @brief Perform SChannel TLS handshake
	 */
	core::Error perform_handshake() noexcept {
		if (!cred_.acquired) return core::Error::HandshakeFailed;

		// Convert hostname to wide string for SChannel
		std::wstring hostname_w;
		auto hv = context_.hostname();
		hostname_w.assign(hv.begin(), hv.end());

		DWORD flags = ISC_REQ_STREAM
		            | ISC_REQ_USE_SUPPLIED_CREDS
		            | ISC_REQ_ALLOCATE_MEMORY
		            | ISC_REQ_CONFIDENTIALITY
		            | ISC_REQ_REPLAY_DETECT
		            | ISC_REQ_SEQUENCE_DETECT;

		// Initial handshake call (no input)
		SecBuffer out_buf;
		out_buf.pvBuffer   = nullptr;
		out_buf.cbBuffer   = 0;
		out_buf.BufferType = SECBUFFER_TOKEN;

		SecBufferDesc out_desc;
		out_desc.ulVersion = SECBUFFER_VERSION;
		out_desc.cBuffers  = 1;
		out_desc.pBuffers  = &out_buf;

		DWORD out_flags = 0;
		TimeStamp lifetime;

		SECURITY_STATUS status = InitializeSecurityContextW(
			&cred_.handle,
			nullptr,
			hostname_w.empty() ? nullptr : hostname_w.data(),
			flags, 0, 0,
			nullptr,
			0,
			&sec_ctx_.handle,
			&out_desc,
			&out_flags,
			&lifetime
		);

		sec_ctx_.initialized = true;

		// Send initial token
		if (out_buf.cbBuffer > 0 && out_buf.pvBuffer) {
			auto token_span = std::span<const uint8_t>(
				static_cast<uint8_t*>(out_buf.pvBuffer), out_buf.cbBuffer);
			socket_.send(token_span);
			FreeContextBuffer(out_buf.pvBuffer);
		}

		// Handshake loop
		std::vector<uint8_t> recv_buf(16384);
		int total_recv = 0;

		while (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE) {
			// Receive server response
			auto recv_span = std::span<uint8_t>(
				recv_buf.data() + total_recv,
				recv_buf.size() - total_recv);
			int received = socket_.recv(recv_span);
			if (received <= 0) return core::Error::HandshakeFailed;
			total_recv += received;

			// Input buffer
			SecBuffer in_buffers[2]{};
			in_buffers[0].pvBuffer   = recv_buf.data();
			in_buffers[0].cbBuffer   = static_cast<unsigned long>(total_recv);
			in_buffers[0].BufferType = SECBUFFER_TOKEN;
			in_buffers[1].BufferType = SECBUFFER_EMPTY;

			SecBufferDesc in_desc;
			in_desc.ulVersion = SECBUFFER_VERSION;
			in_desc.cBuffers  = 2;
			in_desc.pBuffers  = in_buffers;

			// Output buffer
			SecBuffer out_buf2;
			out_buf2.pvBuffer   = nullptr;
			out_buf2.cbBuffer   = 0;
			out_buf2.BufferType = SECBUFFER_TOKEN;

			SecBufferDesc out_desc2;
			out_desc2.ulVersion = SECBUFFER_VERSION;
			out_desc2.cBuffers  = 1;
			out_desc2.pBuffers  = &out_buf2;

			status = InitializeSecurityContextW(
				&cred_.handle,
				&sec_ctx_.handle,
				hostname_w.empty() ? nullptr : hostname_w.data(),
				flags, 0, 0,
				&in_desc,
				0,
				nullptr,
				&out_desc2,
				&out_flags,
				&lifetime
			);

			// Send any tokens back to server
			if (out_buf2.cbBuffer > 0 && out_buf2.pvBuffer) {
				auto token_span = std::span<const uint8_t>(
					static_cast<uint8_t*>(out_buf2.pvBuffer), out_buf2.cbBuffer);
				socket_.send(token_span);
				FreeContextBuffer(out_buf2.pvBuffer);
			}

			// Handle extra data
			if (in_buffers[1].BufferType == SECBUFFER_EXTRA) {
				auto extra_size = in_buffers[1].cbBuffer;
				std::memmove(recv_buf.data(),
					recv_buf.data() + total_recv - extra_size,
					extra_size);
				total_recv = static_cast<int>(extra_size);
			} else if (status != SEC_E_INCOMPLETE_MESSAGE) {
				total_recv = 0;
			}
		}

		if (status != SEC_E_OK) return core::Error::HandshakeFailed;

		// Query stream sizes for send/recv
		QueryContextAttributes(&sec_ctx_.handle,
			SECPKG_ATTR_STREAM_SIZES, &stream_sizes_);

		handshake_done_ = true;
		return core::Error::None;
	}
#else
	core::Error perform_handshake() noexcept {
		// POSIX stub — TLS not yet supported
		handshake_done_ = true; // Passthrough mode
		return core::Error::None;
	}
#endif
};

} // namespace security
} // namespace etherz
