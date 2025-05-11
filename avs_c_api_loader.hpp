// Copyright (c) 2025 Asd-g
// SPDX-License-Identifier: MPL-2.0

#pragma once

namespace avs_loader_meta
{
    constexpr int MAJOR_VERSION = 1;
    constexpr int MINOR_VERSION = 1;
    constexpr int PATCH_VERSION = 0;
    constexpr const char* VERSION_STRING = "1.1.0";
} // namespace avs_loader_meta

#include <atomic>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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

#include <avisynth_c.h>

struct avisynth_c_api_pointers
{
#define FUNC(name)                    \
    using _type_##name = name##_func; \
    _type_##name name;
#include "avs_c_api_functions.inc"
#undef FUNC
};

class avisynth_c_api_loader
{
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
    static const avisynth_c_api_pointers* get_api(AVS_ScriptEnvironment* env, const int required_interface_version,
        const int required_bugfix_version, const std::initializer_list<std::string_view>& required_function_names);

    /**
     * @brief Gets the last error message if get_api returned nullptr.
     * @return A static C-string pointer containing the error description. Valid until the next call to get_api.
     */
    static const char* get_last_error();

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
    template<typename T>
    bool load_single_function(const char* name, T& ptr_member, bool required);

    void* library_handle_{};
    avisynth_c_api_pointers api_pointers_{};
    std::string last_error_message_;
    bool initialized_{};

    static std::atomic<long> ref_count_;
    static avisynth_c_api_loader instance_;

    static void AVSC_CC cleanup_callback(void* user_data, AVS_ScriptEnvironment* env);
};

// Global access point (convenience)
// Initialized by the first successful call to avisynth_c_api_loader::get_api
// Usage: if (g_avs_api) { g_avs_api->avs_add_function(...); } else { /* Handle API load error */ }
inline const avisynth_c_api_pointers* g_avs_api{};

// --- Helper Structs and Functions ---
namespace avs_helpers
{
    // --- RAII Deleters and Smart Pointers ---

    /**
     * @brief Deleter for AVS_Clip, to be used with std::unique_ptr.
     * Calls g_avs_api->avs_release_clip on destruction.
     */
    struct avs_clip_deleter
    {
        void operator()(AVS_Clip* clip) const noexcept
        {
            g_avs_api->avs_release_clip(clip);
        }
    };
    /** @brief std::unique_ptr alias for an AVS_Clip managed by avs_clip_deleter. */
    using avs_clip_ptr = std::unique_ptr<AVS_Clip, avs_clip_deleter>;

    /**
     * @brief Deleter for AVS_VideoFrame, to be used with std::unique_ptr.
     * Calls g_avs_api->avs_release_video_frame on destruction.
     */
    struct avs_video_frame_deleter
    {
        void operator()(AVS_VideoFrame* frame) const noexcept
        {
            g_avs_api->avs_release_video_frame(frame);
        }
    };
    /** @brief std::unique_ptr alias for an AVS_VideoFrame managed by avs_video_frame_deleter. */
    using avs_video_frame_ptr = std::unique_ptr<AVS_VideoFrame, avs_video_frame_deleter>;

    /**
     * @brief Deleter for memory allocated by avs_pool_allocate, to be used with std::unique_ptr.
     * Calls g_avs_api->avs_pool_free on destruction. Requires the AVS_ScriptEnvironment
     * used for allocation to be provided.
     */
    struct avs_pool_deleter
    {
        AVS_ScriptEnvironment* env{};
        void operator()(void* ptr) const noexcept
        {
            if (ptr && env)
            {
                g_avs_api->avs_pool_free(env, ptr);
            }
        }
    };
    /** @brief std::unique_ptr alias for memory from avs_pool_allocate managed by avs_pool_deleter. */
    using avs_pool_ptr = std::unique_ptr<std::byte[], avs_pool_deleter>;

    /**
     * @brief RAII wrapper for an AVS_Value.
     * Ensures g_avs_api->avs_release_value is called when the guard goes out of scope,
     * if it owns the AVS_Value. Supports move semantics.
     */
    class avs_value_guard
    {
    public:
        /**
         * @brief Default constructor. Initializes to a non-owning, avs_void state.
         */
        avs_value_guard()
            : value_(avs_void), owns_value_(false)
        {
        }

        /**
         * @brief Constructs a guard taking ownership of the provided AVS_Value.
         * @param val The AVS_Value to manage. It is assumed that this value may require releasing.
         */
        explicit avs_value_guard(AVS_Value val)
            : value_(val), owns_value_(true)
        {
        }

        /**
         * @brief Destructor. Releases the managed AVS_Value if owned.
         */
        ~avs_value_guard()
        {
            if (owns_value_)
                g_avs_api->avs_release_value(value_);
        }

        avs_value_guard(const avs_value_guard&) = delete;
        avs_value_guard& operator=(const avs_value_guard&) = delete;

        /**
         * @brief Move constructor. Transfers ownership from 'other'.
         * @param other The avs_value_guard to move from. 'other' is left in a non-owning, void state.
         */
        avs_value_guard(avs_value_guard&& other) noexcept
            : value_(other.value_), owns_value_(other.owns_value_)
        {
            other.owns_value_ = false;
            other.value_ = avs_void;
        }

        /**
         * @brief Move assignment operator. Transfers ownership from 'other'.
         * Releases any currently owned value before taking ownership from 'other'.
         * @param other The avs_value_guard to move from. 'other' is left in a non-owning, void state.
         * @return *this
         */
        avs_value_guard& operator=(avs_value_guard&& other) noexcept
        {
            if (this != &other)
            {
                if (owns_value_)
                    g_avs_api->avs_release_value(value_);

                value_ = other.value_;
                owns_value_ = other.owns_value_;
                other.owns_value_ = false;
                other.value_ = avs_void;
            }

            return *this;
        }

        /**
         * @brief Gets a copy of the managed AVS_Value. Does not affect ownership.
         * @return A copy of the internal AVS_Value.
         */
        AVS_Value get() const
        {
            return value_;
        }

        /**
         * @brief Releases ownership of the managed AVS_Value and returns it.
         * The caller is now responsible for calling avs_release_value on the returned AVS_Value.
         * The guard is reset to a non-owning, void state.
         * @return The previously managed AVS_Value.
         */
        AVS_Value release()
        {
            owns_value_ = false;
            AVS_Value temp = value_;
            value_ = avs_void;

            return temp;
        }

        /**
         * @brief Resets the guard to manage a new AVS_Value, taking ownership.
         * Releases any previously owned value.
         * @param new_val The new AVS_Value to manage.
         */
        void reset(AVS_Value new_val = avs_void)
        {
            if (owns_value_)
                g_avs_api->avs_release_value(value_);

            value_ = new_val;
            owns_value_ = (new_val.type == 'v' && new_val.array_size == 0) ? false : true;
        }

        /**
         * @brief Resets the guard to a non-owning, void state.
         * Releases any previously owned value.
         */
        void reset()
        {
            if (owns_value_)
                g_avs_api->avs_release_value(value_);

            value_ = avs_void;
            owns_value_ = false;
        }

    private:
        AVS_Value value_;
        bool owns_value_;
    };

    // --- Argument Parsing Helper ---

    /**
     * @brief Retrieves an optional argument from AVS_Value args. *
     * If the argument at the given index is not defined by the user, std::nullopt is returned.
     * @tparam T The C++ type to convert the argument to (e.g., int, double, std::string, avs_clip_ptr).
     * @param env The AVS_ScriptEnvironment pointer (needed for some conversions like avs_get_clip).
     * @param args The AVS_Value containing the array of arguments.
     * @param index The 0-based index of the argument in the 'args' array.
     * @return std::optional<T> containing the value if present and convertible, otherwise std::nullopt.
     */
    template<typename T>
    AVS_FORCEINLINE std::optional<T> get_opt_arg(AVS_ScriptEnvironment* env, AVS_Value args, int index)
    {
        if (index < 0)
            return std::nullopt;

        AVS_Value val{avs_array_elt(args, index)};

        if (!avs_defined(val))
            return std::nullopt;

        if constexpr (std::is_same_v<T, int>)
            return avs_as_int(val);
        else if constexpr (std::is_same_v<T, bool>)
            return avs_as_bool(val);
        else if constexpr (std::is_same_v<T, double>)
            return avs_as_float(val);
        else if constexpr (std::is_same_v<T, float>)
            return static_cast<float>(avs_as_float(val));
        else if constexpr (std::is_same_v<T, const char*>)
            return avs_as_string(val);
        else if constexpr (std::is_same_v<T, std::string>)
            return std::string(avs_as_string(val));
        else if constexpr (std::is_same_v<T, std::string_view>)
            return std::string_view(avs_as_string(val));
        else if constexpr (std::is_same_v<T, avs_clip_ptr>)
            return avs_clip_ptr(g_avs_api->avs_take_clip(val, env));
        else if constexpr (std::is_same_v<T, AVS_Value>)
            return val;
        else
            static_assert(std::false_type::value, "get_opt_arg: Unsupported type T");

        return std::nullopt;
    }

    template<typename ElementType>
    struct converted_array
    {
        std::unique_ptr<ElementType[]> data;
        int size = 0;

        converted_array()
            : data(nullptr), size(0)
        {
        }

        converted_array(std::unique_ptr<ElementType[]> d, int s)
            : data(std::move(d)), size(s)
        {
        }

        explicit operator bool() const
        {
            return data != nullptr && size > 0;
        }

        bool empty() const
        {
            return size == 0;
        }
    };

    /**
     * @brief Retrieves an optional array argument from AVS_Value args and converts its elements
     *        to a std::unique_ptr<ElementType[]>. *
     * If the argument at the given index is not defined by the user an empty result
     * (data=nullptr, size=0, or an empty unique_ptr with size=0) is returned.
     * @tparam ElementType The C++ type to convert array elements to (e.g., double, int, float).
     * @param args The AVS_Value containing the array of arguments.
     * @param index The 0-based index of the array argument.
     * @return converted_array<ElementType> containing the unique_ptr to the
     *         converted data and its size. If the argument was not present,
     *         or an empty array, the 'data' member might be a unique_ptr to a zero-sized array
     *         or nullptr, and 'size' will be 0.
     */
    template<typename ElementType = double>
    inline converted_array<ElementType> get_opt_array_as_unique_ptr(AVS_Value args, int index)
    {
        AVS_Value array_arg_val{avs_array_elt(args, index)};

        if (!avs_defined(array_arg_val) || !avs_is_array(array_arg_val))
            return {};

        const int array_size{avs_array_size(array_arg_val)};
        auto data_ptr{std::make_unique<ElementType[]>(array_size)};
        const AVS_Value* avs_array_ptr{avs_as_array(array_arg_val)};

        for (int i{0}; i < array_size; ++i)
        {
            const AVS_Value& element_val{avs_array_ptr[i]};

            if constexpr (std::is_same_v<ElementType, double>)
                data_ptr[i] = avs_as_float(element_val);
            else if constexpr (std::is_same_v<ElementType, float>)
                data_ptr[i] = static_cast<float>(avs_as_float(element_val));
            else if constexpr (std::is_same_v<ElementType, bool>)
                data_ptr[i] = avs_as_bool(element_val);
            else if constexpr (std::is_same_v<ElementType, int>)
                data_ptr[i] = avs_as_int(element_val);
            else
                static_assert(std::is_void_v<ElementType> && !std::is_void_v<ElementType>,
                    "get_opt_array_as_unique_ptr: Unsupported ElementType for array conversion.");
        }

        return {std::move(data_ptr), array_size};
    }

    /**
     * @brief Retrieves an optional array argument from AVS_Value args and converts its elements
     *        to a std::vector<ElementType>. *
     * If the argument at the given index is not defined by the user an empty result
     * (data=nullptr, size=0, or an empty vector with size=0) is returned.
     * @tparam ElementType The C++ type to convert array elements to (e.g., double, int, std::string, avs_clip_ptr).
     * @param env The AVS_ScriptEnvironment pointer (needed for some conversions like avs_get_clip).
     * @param args The AVS_Value containing the array of arguments.
     * @param index The 0-based index of the array argument.
     * @return std::vector<ElementType>.
     *         If the argument was not present, or an empty array, the vector will be empty.
     */
    template<typename ElementType = double>
    inline std::vector<ElementType> get_opt_array_as_vector(AVS_ScriptEnvironment* env, AVS_Value args, int index)
    {
        AVS_Value array_arg_val{avs_array_elt(args, index)};

        if (!avs_defined(array_arg_val) || !avs_is_array(array_arg_val))
            return {};

        const int array_size{avs_array_size(array_arg_val)};
        std::vector<ElementType> vec;
        vec.reserve(array_size);
        const AVS_Value* avs_array_ptr{avs_as_array(array_arg_val)};

        for (int i{0}; i < array_size; ++i)
        {
            const AVS_Value& element_val{avs_array_ptr[i]};

            if constexpr (std::is_same_v<ElementType, double>)
                vec.emplace_back(avs_as_float(element_val));
            else if constexpr (std::is_same_v<ElementType, float>)
                vec.emplace_back(static_cast<float>(avs_as_float(element_val)));
            else if constexpr (std::is_same_v<ElementType, int>)
                vec.emplace_back(avs_as_int(element_val));
            else if constexpr (std::is_same_v<ElementType, bool>)
                vec.emplace_back(avs_as_bool(element_val));
            else if constexpr (std::is_same_v<ElementType, const char*>)
                vec.emplace_back(avs_as_string(element_val));
            else if constexpr (std::is_same_v<ElementType, std::string_view>)
                vec.emplace_back(avs_as_string(element_val));
            else if constexpr (std::is_same_v<ElementType, std::string>)
                vec.emplace_back(avs_as_string(element_val));
            else if constexpr (std::is_same_v<ElementType, avs_clip_ptr>)
                vec.emplace_back(g_avs_api->avs_take_clip(element_val, env));
            else
                static_assert(std::is_void_v<ElementType> && !std::is_void_v<ElementType>,
                    "get_opt_array_as_vector: Unsupported ElementType for array conversion.");
        }

        return vec;
    }
} // namespace avs_helpers
