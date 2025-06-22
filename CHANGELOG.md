# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2025-06-22

### Added
- **CMake Build System:**
    - The project is now a CMake project.
    - Supports two usage models:
        1.  **Submodule:** Can be built directly from source via `add_subdirectory`.
        2.  **System Package:** Can be installed and then found via `find_package(avs_c_api_loader)`.
    - Provides an imported `ALIAS` target `avs_c_api_loader::avs_c_api_loader` for consistent linking.
    - Includes a `FindAvisynthPlus.cmake` module to automatically locate the required Avisynth+ headers.

## [1.1.0] - 2025-05-11

### Added
- **RAII Wrappers (in `avs_loader_utils` namespace):**
    - `avs_clip_ptr`: `std::unique_ptr` for `AVS_Clip` with automatic `avs_release_clip`.
    - `avs_video_frame_ptr`: `std::unique_ptr` for `AVS_VideoFrame` with automatic `avs_release_video_frame`.
    - `avs_pool_ptr`: `std::unique_ptr` for memory from `avs_pool_allocate` with automatic `avs_pool_free`.
    - `avs_value_guard`: RAII wrapper for `AVS_Value` ensuring `avs_release_value` is called.
- **Argument Parsing Utilities (in `avs_loader_utils` namespace):**
    - `get_opt_arg<T>`: Retrieves an optional argument, returning `std::optional<T>`. Throws `AvisynthApiNotReadyError` on critical API issues.
    - `get_opt_array_as_unique_ptr<T>`: Converts an optional array argument to `converted_array<T>` (holding `std::unique_ptr<T[]>`).
    - `get_opt_array_as_vector<T>`: Converts an optional array argument to `std::vector<T>` (suitable for numbers, `std::string`, `avs_clip_ptr`).

## [1.0.0] - 2025-04-05

### Added
- Initial release.
- Core functionality for dynamic loading of Avisynth+ C API:
    - `avisynth_c_api_loader` class with `get_api()` method.
    - `g_avs_api` global pointer.
    - Version checking and reference counting for library management.
