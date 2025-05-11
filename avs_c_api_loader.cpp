// Copyright (c) 2025 Asd-g
// SPDX-License-Identifier: MPL-2.0

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "avs_c_api_loader.hpp"

#ifdef _WIN32
using LibHandle = HMODULE;
constexpr const wchar_t* AVS_LIB_NAME{L"avisynth.dll"};

inline void* avs_open_library()
{
    return LoadLibraryW(AVS_LIB_NAME);
}
inline void avs_close_library(void* const handle)
{
    if (handle)
        FreeLibrary(static_cast<LibHandle>(handle));
}
inline void* avs_get_proc_address(void* handle, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<LibHandle>(handle), name));
}
#else
#include <dlfcn.h>
using LibHandle = void*;
#if __APPLE__
constexpr const char* AVS_LIB_NAME{"libavisynth.dylib"};
#else
constexpr const char* AVS_LIB_NAME{"libavisynth.so"};
#endif

inline void* avs_open_library()
{
    return dlopen(AVS_LIB_NAME, RTLD_LAZY | RTLD_LOCAL);
}
inline void avs_close_library(void* const handle)
{
    if (handle)
        dlclose(handle);
}
inline void* avs_get_proc_address(void* handle, const char* name)
{
    return dlsym(handle, name);
}
#endif

std::atomic<long> avisynth_c_api_loader::ref_count_{0};
avisynth_c_api_loader avisynth_c_api_loader::instance_;

template<typename T>
bool avisynth_c_api_loader::load_single_function(const char* name, T& ptr_member, bool required)
{
    void* addr{avs_get_proc_address(library_handle_, name)};
    if (addr)
    {
        ptr_member = reinterpret_cast<T>(addr);
        return true;
    }
    else
    {
        ptr_member = nullptr;
        if (required)
        {
            last_error_message_ = "Failed to load required function: ";
            last_error_message_ += name;
            return false;
        }
        return true; // Optional function not found is not a failure for the loader itself
    }
}

bool avisynth_c_api_loader::load_functions(AVS_ScriptEnvironment* env, const int required_interface_version,
    const int required_bugfix_version, const std::initializer_list<std::string_view>& required_names)
{
    // --- Load Library ---
    library_handle_ = avs_open_library();
    if (!library_handle_)
    {
        last_error_message_ = "Failed to load avisynth library (";
#ifdef _WIN32
        last_error_message_ += "avisynth.dll";
#elif __APPLE__
        last_error_message_ += "libavisynth.dylib";
#else
        last_error_message_ += "libavisynth.so";
#endif
        last_error_message_ += "). Is Avisynth+ installed correctly?";
        return false;
    }

    // --- Load Loader Essentials (check_version, at_exit) ---
    bool essentials_ok{true};
    essentials_ok &= load_single_function("avs_check_version", api_pointers_.avs_check_version, true);
    essentials_ok &= load_single_function("avs_at_exit", api_pointers_.avs_at_exit, true);
    essentials_ok &= load_single_function("avs_get_env_property", api_pointers_.avs_get_env_property, false);

    if (!essentials_ok)
    {
        // Error message set by load_single_function
        unload_library();
        return false;
    }

    // --- Version Check ---
    int host_major_version{};
    int host_bugfix_version{};

    bool version_ok{[&]() {
        if (api_pointers_.avs_get_env_property)
        {
            host_major_version = static_cast<int>(api_pointers_.avs_get_env_property(env, AVS_AEP_INTERFACE_VERSION));
            host_bugfix_version = static_cast<int>(api_pointers_.avs_get_env_property(env, AVS_AEP_INTERFACE_BUGFIX));
            if (host_major_version > required_interface_version)
                return true;
            else if (host_major_version == required_interface_version)
                return (host_bugfix_version >= required_bugfix_version);
            else
                return false;
        }
        else
        {
            // We can't know the exact host versions here, use required as placeholder if ok.
            host_major_version = required_interface_version;
            host_bugfix_version = 0;
            return (!api_pointers_.avs_check_version(env, required_interface_version));
        }
    }()};

    if (!version_ok)
    {
        static char version_error_msg[200]; // Static buffer for safety
        if (api_pointers_.avs_get_env_property)
            snprintf(version_error_msg, sizeof(version_error_msg),
                "Avisynth C API Error: Plugin requires interface >= %d.%d, but found %d.%d.", required_interface_version,
                required_bugfix_version, host_major_version, host_bugfix_version);
        else
            // Less precise error message
            snprintf(version_error_msg, sizeof(version_error_msg),
                "Avisynth C API Error: Plugin requires interface >= %d.%d, but the installed AviSynth+ version is too old.",
                required_interface_version, required_bugfix_version);
        last_error_message_ = version_error_msg;
        unload_library();
        return false;
    }

    // --- Load functions explicitly required by the plugin ---
    // Build a map from name to member pointer offset for efficient lookup
    std::unordered_map<std::string_view, size_t> pointer_offset_map{};
    avisynth_c_api_pointers* const base_ptr{&api_pointers_}; // Base address for offset calculation
#define FUNC(name) pointer_offset_map[#name] = reinterpret_cast<char*>(&(api_pointers_.name)) - reinterpret_cast<char*>(base_ptr);
#include "avs_c_api_functions.inc"
#undef FUNC

    for (const std::string_view req_name_sv : required_names)
    {
        auto it{pointer_offset_map.find(req_name_sv)};
        if (it != pointer_offset_map.end())
        {
            // Calculate the address of the member pointer using the offset
            void** target_member_ptr_addr{reinterpret_cast<void**>(reinterpret_cast<char*>(base_ptr) + it->second)};
            // Load the function using its name and store it via the calculated address
            if (!load_single_function(req_name_sv.data(), *target_member_ptr_addr, true))
            {
                // Error set by load_single_function
                unload_library();
                return false;
            }
        }
        else
        {
            // This indicates an error in the plugin's required_function_names list
            last_error_message_ = "Internal Error: Unknown function requested as required: ";
            last_error_message_ += req_name_sv;
            unload_library();
            return false;
        }
    }

    // Load all other known functions optionally ---
#define FUNC(name)                                              \
    if (api_pointers_.name == nullptr)                          \
    { /* Load only if not already loaded as required */         \
        load_single_function(#name, api_pointers_.name, false); \
    }
#include "avs_c_api_functions.inc"
#undef FUNC

    // --- Register Cleanup ---
    api_pointers_.avs_at_exit(env, avisynth_c_api_loader::cleanup_callback, nullptr);

    initialized_ = true;
    last_error_message_.clear();
    return true;
}

// --- Unloading and Cleanup ---

void avisynth_c_api_loader::unload_library()
{
    if (library_handle_)
    {
        avs_close_library(library_handle_);
        library_handle_ = nullptr;
        api_pointers_ = {};
        initialized_ = false;
        g_avs_api = nullptr;
    }
}

void AVSC_CC avisynth_c_api_loader::cleanup_callback(void* /*user_data*/, AVS_ScriptEnvironment* /*env*/)
{
    // This is called by Avisynth when the environment is destroyed.
    // Decrement ref count. If it reaches zero, unload the library.
    if (ref_count_.fetch_sub(1, std::memory_order_release) == 1)
    {
        std::atomic_thread_fence(std::memory_order_acquire); // Ensure visibility of state changes
        instance_.unload_library();
    }
}

const avisynth_c_api_pointers* avisynth_c_api_loader::get_api(AVS_ScriptEnvironment* env, const int required_interface_version,
    const int required_bugfix_version, const std::initializer_list<std::string_view>& required_function_names)
{
    // Increment reference count first.
    // If this is the first call (count was 0 before increment), initialize.
    if (instance_.ref_count_.fetch_add(1, std::memory_order_relaxed) == 0)
    {
        // Perform the full load (required + optional)
        if (!instance_.load_functions(env, required_interface_version, required_bugfix_version, required_function_names))
        {
            // Loading failed. Decrement count back and return null.
            instance_.ref_count_.fetch_sub(1, std::memory_order_relaxed);
            g_avs_api = nullptr; // Ensure global pointer is null
            return nullptr;      // Error message is in instance_.last_error_message_
        }
    }
    else
    {
        // Already initialized
        // Crucially, check if the *already loaded* version meets the *current request's* requirements.
        if (instance_.initialized_)
        {
            int loaded_major_version{};
            int loaded_bugfix_version{};

            bool version_ok{[&]() {
                if (instance_.api_pointers_.avs_get_env_property)
                {
                    loaded_major_version = static_cast<int>(instance_.api_pointers_.avs_get_env_property(env, AVS_AEP_INTERFACE_VERSION));
                    loaded_bugfix_version = static_cast<int>(instance_.api_pointers_.avs_get_env_property(env, AVS_AEP_INTERFACE_BUGFIX));
                    if (loaded_major_version > required_interface_version)
                        return true;
                    else if (loaded_major_version == required_interface_version)
                        return (loaded_bugfix_version >= required_bugfix_version);
                    else
                        return false;
                }
                else
                {
                    // We can't know the exact host versions here, use required as placeholder if ok.
                    loaded_major_version = required_interface_version;
                    loaded_bugfix_version = 0;
                    return (!instance_.api_pointers_.avs_check_version(env, required_interface_version));
                }
            }()};

            if (!version_ok)
            {
                static char version_error_msg[200]; // Static buffer for safety
                if (instance_.api_pointers_.avs_get_env_property)
                    snprintf(version_error_msg, sizeof(version_error_msg),
                        "Avisynth C API Error: Plugin requires interface >= %d.%d, but found %d.%d.", required_interface_version,
                        required_bugfix_version, loaded_major_version, loaded_bugfix_version);
                else
                    // Less precise error message
                    snprintf(version_error_msg, sizeof(version_error_msg),
                        "Avisynth C API Error: Plugin requires interface >= %d.%d, but the installed AviSynth+ version is too old.",
                        required_interface_version, required_bugfix_version);
                instance_.last_error_message_ = version_error_msg;
                instance_.unload_library();
                return nullptr;
            }
        }
        else if (!instance_.initialized_)
        {
            // This might happen if called concurrently before initialization finishes?
            // Let's return the current error message from the instance.
            // The caller should retry or handle the failure.
            // Decrement the count we just added.
            cleanup_callback(nullptr, env);
            return nullptr; // Error message should reflect the initial failure.
        }
    }

    g_avs_api = &instance_.api_pointers_; // Set global pointer
    return g_avs_api;
}

const char* avisynth_c_api_loader::get_last_error()
{
    // Copy the std::string message to a static buffer to ensure C-string lifetime.
    static char static_error_buffer[300]; // Increased size
    if (!instance_.last_error_message_.empty())
    {
        strncpy(static_error_buffer, instance_.last_error_message_.c_str(), sizeof(static_error_buffer) - 1);
        static_error_buffer[sizeof(static_error_buffer) - 1] = '\0';
        return static_error_buffer;
    }

    return "Unknown Avisynth C API loading error."; // Default fallback
}
