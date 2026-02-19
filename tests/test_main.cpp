/**
 * @file test_main.cpp
 * @brief Test runner entry point for Etherz test suite
 */

#include "test_framework.hpp"

#ifdef _WIN32
	#include <windows.h>
#endif

int main() {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif
	return etherz_test::run_all();
}
