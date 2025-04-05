## Description

Helper for AviSynthPlus C API dynamic loading.

#### Usage:

```
#include "avs_c_api_loader.hpp"

static AVS_Value AVSC_CC Create_filter(AVS_ScriptEnvironment* env, AVS_Value args, void* param)
{
    AVS_FilterInfo* fi;
    AVS_Clip* clip{ g_avs_api->avs_new_c_filter(env, &fi, avs_array_elt(args, 0), 1) };
}

const char* AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment* env)
{
    // Minimum Avisynth+ C API interface version required by this plugin.
    static constexpr int REQUIRED_INTERFACE_VERSION{ 10 };

    // Minimum bugfix version for the specified interface version.
    static constexpr int REQUIRED_BUGFIX_VERSION{ 0 };

    // List the C API functions absolutely essential for this plugin to operate.
    // Loading will fail if any of these cannot be found.
    static constexpr std::initializer_list<std::string_view> required_functions
    {
        // Functions often needed for basic filter operation:
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
        "avs_release_video_frame",
        "avs_release_clip",
        "avs_set_to_clip",
        "avs_set_to_error",
        "avs_new_c_filter",
        "avs_add_function"
    };

    // This call initializes the loader (if not already done), checks requirements,
    // and sets the global g_avs_api pointer on success.
    // It returns nullptr on fail.
    if (!avisynth_c_api_loader::get_api(env, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, required_functions))
    {
        std::cerr << avisynth_c_api_loader::get_last_error() << std::endl;
        return avisynth_c_api_loader::get_last_error();
    }

    g_avs_api->avs_add_function(env, "filter", "ci", Create_filter, nullptr);

    return "filter success";
```
