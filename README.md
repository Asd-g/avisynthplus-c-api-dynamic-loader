## Description

A C++20 helper library for dynamically loading the AviSynthPlus C API. This allows plugins to avoid static linking against `AviSynth.lib`, potentially improving portability and reducing dependencies on specific Avisynth+ versions at compile time.

The loader handles:
- Dynamically loading the Avisynth library.
- Retrieving function pointers for the C API.
- Version checking against required interface and bugfix versions.
- Reference counting to ensure the library is unloaded only when no longer in use by any plugin instance utilizing this loader.
- Providing a global pointer `g_avs_api` to the loaded function table.
- Offering convenient C++ RAII wrappers (in `avs_loader_utils` namespace):
    - `avs_clip_ptr`: `std::unique_ptr` for `AVS_Clip` with automatic `avs_release_clip`.
    - `avs_video_frame_ptr`: `std::unique_ptr` for `AVS_VideoFrame` with automatic `avs_release_video_frame`.
    - `avs_pool_ptr`: `std::unique_ptr` for memory from `avs_pool_allocate` with automatic `avs_pool_free`.
    - `avs_value_guard`: RAII wrapper for `AVS_Value` ensuring `avs_release_value` is called.
- Offering convenient argument parsing utilities (in `avs_loader_utils` namespace):
    - `get_opt_arg<T>`: Retrieves an optional argument, returning `std::optional<T>`.
    - `get_opt_array_as_unique_ptr<T>`: Converts an optional array argument to `converted_array<T>` (holding `std::unique_ptr<T[]>`) (suitable for primitive values).
    - `get_opt_array_as_vector<T>`: Converts an optional array argument to `std::vector<T>` (suitable for numbers, `std::string`, `avs_clip_ptr`).

---

## Building and Integration

This library is designed to be integrated into your plugin's build process using its CMake build system. It supports two primary methods of consumption:

1.  **As a Git Submodule (Recommended for most plugins)**
2.  **As a Pre-installed System Library**

In both cases, the library provides an imported `ALIAS` target `avs_c_api_loader::avs_c_api_loader` for easy and consistent linking.

### Method 1: Using as a Git Submodule

This is the simplest way to get started. Add `avs_c_api_loader` as a submodule to your project, for example, in an `external/` directory.

**Your Plugin's `CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyAvisynthPlugin LANGUAGES CXX)

# 1. Find the Avisynth+ headers needed by the helper.
#    The helper provides a FindAvisynthPlus.cmake module for this.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/avs_c_api_loader/cmake")
find_package(AvisynthPlus REQUIRED)

# 2. Add the helper library as a subdirectory.
add_subdirectory(external/avs_c_api_loader)

# 3. Define your plugin target.
add_library(my_plugin MODULE src/my_plugin.cpp)

# 4. Link to the helper.
#    This automatically handles include paths and dependencies.
target_link_libraries(my_plugin PRIVATE avs_c_api_loader::avs_c_api_loader)
```

### Method 2: Using as an Installed Library

This method is for advanced users who may want to share a single build of the helper library across multiple projects.

**Step A: Build and Install `avs_c_api_loader`**

First, build and install the helper library to a location of your choice.

```bash
# Clone the repository
git clone https://github.com/Asd-g/avisynthplus-c-api-dynamic-loader avs_c_api_loader
cd avs_c_api_loader

# Configure, build, and install.
# You must tell CMake where to find the Avisynth+ headers for the build.
cmake -B build -D CMAKE_INSTALL_PREFIX=C:/dev/install -D CMAKE_PREFIX_PATH=C:/path/to/avisynth/sdk
cmake --build build
cmake --install build
```

**Step B: Your Plugin's `CMakeLists.txt`**

Your plugin can now find the pre-installed package using `find_package`.

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyAvisynthPlugin LANGUAGES CXX)

# 1. Find the pre-installed helper library.
#    The helper's own config file will find its dependency (Avisynth+ headers).
find_package(avs_c_api_loader REQUIRED)

# 2. Define your plugin target.
add_library(my_plugin MODULE src/my_plugin.cpp)

# 3. Link to the helper.
target_link_libraries(my_plugin PRIVATE avs_c_api_loader::avs_c_api_loader)
```

When configuring your plugin, you must tell CMake where to find the installed helper:

```bash
cmake -B build -D CMAKE_PREFIX_PATH=C:/dev/install
```

### Providing a User-Friendly Switch

You can combine both methods in your plugin's `CMakeLists.txt` to give users the choice:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyAvisynthPlugin LANGUAGES CXX)

# Add a user-configurable option
option(USE_SYSTEM_AVS_HELPER "Use an installed version of the helper" OFF)

if(USE_SYSTEM_AVS_HELPER)
    find_package(avs_c_api_loader REQUIRED)
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/avs_c_api_loader/cmake")
    find_package(AvisynthPlus REQUIRED)
    add_subdirectory(external/avs_c_api_loader)
endif()

# ...
target_link_libraries(my_plugin PRIVATE avs_c_api_loader::avs_c_api_loader)
```

---

#### Usage:

```
#include <iostream>

#include "avs_c_api_loader.hpp"

struct my_filter_data
{
    // Example members
    int some_int_value;
    std::string some_string_value;
    std::vector<double> float_array_values;
    std::vector<avs_clip_ptr> other_clips;
    avs_clip_ptr mask_clip;
};



static AVS_Value AVSC_CC create_filter(AVS_ScriptEnvironment* env, AVS_Value args, void* param)
{
    AVS_FilterInfo* fi{};
    // Use avs_clip_ptr for the filter's main child clip
    avs_helpers::avs_clip_ptr clip_ref(g_avs_api->avs_new_c_filter(env, &fi, avs_array_elt(args, 0), 1));

    std::unique_ptr<my_filter_data> filter_data{std::make_unique<my_filter_data>()};

    // Argument 1: Optional integer 'some_int_value' (default to 10)
    filter_data->some_int_value = avs_helpers::get_opt_arg<int>(env, args, 1).value_or(10);

    // Argument 2: Optional string 'some_string_value' (default to "default")
    filter_data->some_string_value = avs_helpers::get_opt_arg<std::string>(env, args, 2).value_or("default");

    // Argument 3: Optional float array 'float_array_values' (f*)
    filter_data->float_array_values = avs_helpers::get_opt_array_as_vector<double>(env, args, 3);

    // Argument 3: Optional float array 'float_array_values' (f*)
    // Example of using std::unique_ptr instead std::vector
    avs_helpers::converted_array<double> float_array_values{avs_helpers::get_opt_array_as_unique_ptr<double>(args, 3)};
    const int post_conv_size{post_conv_load.size};

    // Argument 4: Optional array of clips 'other_clips' (c*)
    filter_data->other_clips = avs_helpers::get_opt_array_as_vector<avs_clip_ptr>(env, args, 4);

    // Argument 5: Optional bool 'show_some_info' (b)
    if (avs_helpers::get_opt_arg<bool>(env, args, 5).value_or(false))
    {
        AVS_Value cl;
        g_avs_api->avs_set_to_clip(&cl, clip_ref.get());
        avs_helpers::avs_value_guard cl_guard(cl); // cl_guard now owns the AVS_Value representing the clip
        AVS_Value args_[2]{cl_guard.get(), avs_new_value_string("some message")};
        AVS_Value v{g_avs_api->avs_invoke(env, "Text", avs_new_value_array(args_, 2), 0)};

        return v;
    }

    fi->user_data = filter_data.release();

    AVS_Value v;
    g_avs_api->avs_set_to_clip(&v, clip_ref.get());

    return v;
}

const char* AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment* env)
{
    // Minimum Avisynth+ C API interface version required by this plugin.
    static constexpr int REQUIRED_INTERFACE_VERSION{ 10 };

    // Minimum bugfix version for the specified interface version.
    static constexpr int REQUIRED_BUGFIX_VERSION{ 0 };

    // List the C API functions absolutely essential for this plugin to operate.
    // Loading will fail if any of these cannot be found.
    static constexpr std::string_view required_functions_storage[]
    {
        // Functions often needed for basic filter operation:
        "avs_pool_free",           // avs loader helper functions
        "avs_release_clip",        // avs loader helper functions
        "avs_release_value",       // avs loader helper functions
        "avs_release_video_frame", // avs loader helper functions
        "avs_take_clip",           // avs loader helper functions
        "avs_get_frame",
        "avs_get_pitch_p",
        "avs_get_row_size_p",
        "avs_get_height_p",
        "avs_get_write_ptr_p",
        "avs_get_read_ptr_p",
        "avs_get_frame_props_rw",
        "avs_get_video_info",
        "avs_get_plane_height_subsampling",
        "avs_get_plane_width_subsampling",
        "avs_is_444",
        "avs_prop_set_float",
        "avs_set_to_clip",
        "avs_set_to_error",
        "avs_new_c_filter",
        "avs_add_function"
    };
    static constexpr std::span<const std::string_view> required_functions{ required_functions_storage };

    // This call initializes the loader (if not already done), checks requirements,
    // and sets the global g_avs_api pointer on success.
    // It returns nullptr on fail.
    if (!avisynth_c_api_loader::get_api(env, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, required_functions))
    {
        std::cerr << avisynth_c_api_loader::get_last_error() << std::endl;
        return avisynth_c_api_loader::get_last_error();
    }

    g_avs_api->avs_add_function(env, "filter", "c"
                                               "[some_int_value]i"
                                               "[some_string_value]s"
                                               "[float_array_values]f*"
                                               "[other_clips]c*"
                                               "[mask_clip]c"
                                               "[show_some_info]b", create_filter, nullptr);

    return "filter success";
```
