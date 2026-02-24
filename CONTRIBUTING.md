# Contributing to UBAC

This document outlines the coding standards and architectural patterns adopted in the UBAC project.

## 1. Project Structure (Layering)
Source files are organized by abstraction layer rather than just file type:
- `main/src/drivers/`: Hardware-specific drivers (ADC, MUX, PWM).
- `main/src/net/`: Networking protocols and services (HTTP, DNS, UDP, Wi-Fi).
- `main/src/app/`: High-level business logic and task management.

Header files follow the same structure in `main/include/`.

## 2. Naming Schemes

### Public API
All public symbols (functions, structs, globals) in header files must be prefixed with their module name to prevent collisions in the global C namespace.
- Example: `ntc_history_init()`, `i2c_manager_bus_handle`.

### Static Variables
Internal state variables marked `static` should use the `s_` prefix.
- Example: `static bool s_ready;`.

### Static Functions
Local functions should be short and descriptive. Full module prefixes are unnecessary but a specific name is preferred over generic ones.
- Example: `compute_crc32()` or `flush_locked()`.

## 3. Error Handling
- Functions that perform I/O or can fail must return `esp_err_t`.
- Use `ESP_ERROR_CHECK()` in the caller for critical initializations.
- Use `ESP_LOGE()` and return error codes for recoverable or logic-level failures.

## 4. Configuration
- Hardware pin defines are consolidated in `main/include/drivers/ubac_board_v1.h`.
- Logic-related constants (intervals, sizes, scales) are managed via `main/Kconfig.projbuild` and accessed via `CONFIG_` macros.

## 5. Documentation
- Every header file starts with a standard GPLv3 copyright notice.
- Use Doxygen-style comments (`/** ... */`) for files (`@file`), functions (`@brief`, `@param`, `@return`), and structs.

## 6. Formatting
- Code is formatted using `clang-format` based on the project's `.clang-format` configuration.
- Run formatting before committing: `find main -name "*.[ch]" -exec clang-format -i {} +`.
