// Copyright (c) 2025 Asd-g
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <atomic>
#include <initializer_list>
#include <string>
#include <string_view>

#define AVSC_NO_DECLSPEC

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "avisynth_c.h"

struct avisynth_c_api_pointers
{
#define FUNC(name) \
    using _type_##name = name##_func; \
    _type_##name name;
#include "avs_c_api_functions.inc"
#undef FUNC
};

class avisynth_c_api_loader
{
private:
    avisynth_c_api_loader() = default;
    ~avisynth_c_api_loader() = default;

    // Delete copy/move operations
    avisynth_c_api_loader(const avisynth_c_api_loader&) = delete;
    avisynth_c_api_loader& operator=(const avisynth_c_api_loader&) = delete;
    avisynth_c_api_loader(avisynth_c_api_loader&&) = delete;
    avisynth_c_api_loader& operator=(avisynth_c_api_loader&&) = delete;

    // Core loading function (called only once)
    bool load_functions(AVS_ScriptEnvironment* env, const int required_interface_version, const int required_bugfix_version,
        const std::initializer_list<std::string_view>& required_names);

    // Unload function (called only once when ref_count hits zero)
    void unload_library();

    // Helper to load a single function pointer
    template <typename T>
    bool load_single_function(const char* name, T& ptr_member, bool required);

    void* library_handle_{};
    avisynth_c_api_pointers api_pointers_{};
    std::string last_error_message_;
    bool initialized_{};

    static std::atomic<long> ref_count_;
    static avisynth_c_api_loader instance_;

    static void AVSC_CC cleanup_callback(void* user_data, AVS_ScriptEnvironment* env);

public:
    /**
     * @brief Gets the singleton instance and ensures the API is loaded.
     * Handles reference counting. Must be called from avisynth_c_plugin_init.
     * @param env The AVS_ScriptEnvironment pointer.
     * @param required_interface_version Minimum AVS_INTERFACE_VERSION needed (e.g., AVS_INTERFACE_VERSION).
     * @param required_bugfix_version Minimum AVISYNTHPLUS_INTERFACE_BUGFIX_VERSION needed for the required_interface_version.
     * @param required_function_names List of function names (e.g., "avs_get_frame") absolutely required by the plugin.
     * @return Pointer to the loaded API pointers on success, nullptr on failure. Check get_last_error() on failure.
     */
    static const avisynth_c_api_pointers* get_api(AVS_ScriptEnvironment* env, const int required_interface_version, const int required_bugfix_version,
        const std::initializer_list<std::string_view>& required_function_names);

    /**
     * @brief Gets the last error message if get_api returned nullptr.
     * @return A static C-string pointer containing the error description. Valid until the next call to get_api.
     */
    static const char* get_last_error();
};

// Global access point (convenience)
// Initialized by the first successful call to avisynth_c_api_loader::get_api
// Usage: if (g_avs_api) { g_avs_api->avs_add_function(...); } else { /* Handle API load error */ }
inline const avisynth_c_api_pointers* g_avs_api{};
