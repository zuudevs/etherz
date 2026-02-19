# Contributing to Etherz

Thank you for your interest in contributing! Here's how to get started.

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/etherz.git
   cd etherz
   ```
3. **Create a branch** for your feature:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

```bash
# Configure with tests and examples
cmake -S . -B build -DETHERZ_BUILD_TESTS=ON -DETHERZ_BUILD_EXAMPLES=ON

# Build
cmake --build build

# Run tests
./bin/etherz_tests      # or .\bin\etherz_tests.exe
```

## Code Style

- **C++23** — Use modern features (`std::print`, concepts, `constexpr`, `<=>`)
- **Header-only** — All code lives in `include/`
- **Namespaces** — Follow `etherz::<module>::` convention
- **RAII** — All resources must be managed with RAII
- **No exceptions** — Use `core::Error` enum for error reporting
- **`display()`** — Every public type should have a `display()` method

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / Structs | PascalCase | `TlsSocket`, `HttpClient` |
| Functions | snake_case | `error_message()`, `ping()` |
| Constants | snake_case | `invalid_socket` |
| Enums | PascalCase values | `Error::ConnectFailed` |
| Files | snake_case.hpp | `tls_socket.hpp` |
| Namespaces | lowercase | `etherz::net` |

## Adding a New Feature

1. Add headers to the appropriate `include/<module>/` directory
2. Update `src/main.cpp` with a demo section
3. Add unit tests in `tests/test_<feature>.cpp`
4. Register test source in `CMakeLists.txt`
5. Update `docs/API.md` and `docs/ARCHITECTURE.md`
6. Add a changelog entry in `docs/CHANGELOG.md`

## Pull Request Process

1. Ensure your code **builds with 0 warnings** (both MSVC and Clang)
2. Ensure all **tests pass** (`./bin/etherz_tests`)
3. Update documentation if you added/changed public APIs
4. Write a clear PR description explaining **what** and **why**
5. Reference any related issues

## Roadmap Items

Check [ROADMAP.md](docs/ROADMAP.md) for open items. Great first contributions include example programs and documentation improvements.

## Code of Conduct

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE.md).
