# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.x   | ✅ Active |
| < 1.0   | ❌ No longer supported |

## Reporting a Vulnerability

If you discover a security vulnerability in Etherz, please report it responsibly:

1. **Email:** [zuudevs@gmail.com](mailto:zuudevs@gmail.com)
2. **Subject:** `[SECURITY] Etherz — Brief description`
3. **Include:**
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

**Please do NOT open a public issue for security vulnerabilities.**

## Response Timeline

| Stage | Timeframe |
|-------|-----------|
| Acknowledgment | Within 48 hours |
| Initial assessment | Within 1 week |
| Fix available | Within 2 weeks (critical) |

## Security Considerations

### TLS/SSL

- Etherz uses **Windows SChannel** for TLS on Windows
- Certificate verification is configurable via `TlsContext::verify_mode`
- Self-signed certificates are supported but should only be used for development
- Always use `TlsVerifyMode::Peer` in production

### Network Input

- All parsers (URL, HTTP, WebSocket) handle malformed input gracefully
- Buffer sizes are bounded to prevent excessive memory allocation
- IP address parsing validates format before construction

### Platform Security

- Socket handles are managed with RAII to prevent resource leaks
- No raw `new`/`delete` — all memory via `std::vector` and `std::array`
- No `unsafe` casts outside of platform abstraction boundaries
