#include "game_package.h"

#include "application/application.h"
#include "core/types.h"
#include "containers/bytes.h"
#include "containers/darray.h"
#include "containers/string.h"
#include "containers/string_builder.h"
#include "platform/platform.h"
#include "serialization/binary.h"
#include "fileio/fileio.h"
#include "engine/shader_paths.h"
#include "game.h"

#include <stb_image.h>

namespace Package
{

Bytes pack_assets()
{
    constexpr u64 GB = 1 * 1024 * 1024 * 1024;
    DynamicArray<u8> bytes = make<DynamicArray<u8>>(GB);

    append(bytes, Binary::OBJECT_START);

    {   // Font
        Bytes font_bytes = file_load_bytes(ref("assets/fonts/assistant-medium.font.bytes"));
        Binary::append_bytes(bytes, font_bytes.data, font_bytes.size);
        free(font_bytes);
    }

    stbi_set_flip_vertically_on_load(true);

    {   // Power Button
        const String image_path = ref("assets/art/power_button.png");

        s32 width, height, bytes_pp;
        u8* pixels = stbi_load(image_path.data, &width, &height, &bytes_pp, 0);

        Binary::append_image(bytes, image_path, pixels, width, height, bytes_pp);

        stbi_image_free(pixels);
    }

    {   // Shortcut Icon Project
        const String image_path = ref("assets/art/shortcut_icon_project.png");

        s32 width, height, bytes_pp;
        u8* pixels = stbi_load(image_path.data, &width, &height, &bytes_pp, 0);

        Binary::append_image(bytes, image_path, pixels, width, height, bytes_pp);

        stbi_image_free(pixels);
    }
    
    {   // Shortcut Icon Settings
        const String image_path = ref("assets/art/shortcut_icon_settings.png");

        s32 width, height, bytes_pp;
        u8* pixels = stbi_load(image_path.data, &width, &height, &bytes_pp, 0);

        Binary::append_image(bytes, image_path, pixels, width, height, bytes_pp);

        stbi_image_free(pixels);
    }

    {   // Shortcut Icon Notes
        const String image_path = ref("assets/art/shortcut_icon_notes.png");

        s32 width, height, bytes_pp;
        u8* pixels = stbi_load(image_path.data, &width, &height, &bytes_pp, 0);

        Binary::append_image(bytes, image_path, pixels, width, height, bytes_pp);

        stbi_image_free(pixels);
    }

    append(bytes, Binary::OBJECT_END);
    
    if (bytes.size != bytes.capacity)
        resize(bytes, bytes.size);  // Shrink the array to free extra memory

    return Bytes { bytes.data, bytes.size };
}

Bytes pack_settings(const Application& app, const GameData& data)
{
    DynamicArray<u8> bytes = make<DynamicArray<u8>>(2048Ui64);

    append(bytes, Binary::OBJECT_START);

    {   // Window Style
        append(bytes, Binary::INTEGER_U32);
        Binary::append_integer(bytes, (u32) app.window.style);
    }

    {   // Border Color
        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.r);

        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.g);
        
        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.b);
    }

    {   // Wallpaper Image
        s32 width    = texture_get_width(data.desktop_wallpaper);
        s32 height   = texture_get_height(data.desktop_wallpaper);
        s32 bytes_pp = texture_get_bytes_pp(data.desktop_wallpaper);
        Binary::append_image(bytes, ref("Wallpaper"), data.wallpaper_pixels, width, height, bytes_pp);
    }

    append(bytes, Binary::OBJECT_END);
    
    if (bytes.size != bytes.capacity)
        resize(bytes, bytes.size);  // Shrink the array to free extra memory

    return Bytes { bytes.data, bytes.size };
}

Bytes pack_settings_default(const GameData& data)
{
    DynamicArray<u8> bytes = make<DynamicArray<u8>>(2048Ui64);

    append(bytes, Binary::OBJECT_START);

    {   // Window Style
        append(bytes, Binary::INTEGER_U32);
        Binary::append_integer(bytes, (u32) WindowStyle::FULLSCREEN);
    }

    {   // Border Color
        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.r);

        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.g);
        
        append(bytes, Binary::FLOAT_32);
        Binary::append_float(bytes, data.border_color.b);
    }

    {   // Wallpaper Image
        stbi_set_flip_vertically_on_load(true);

        s32 width, height, bytes_pp;
        u8* pixels = stbi_load("assets/art/wallpaper_default.png", &width, &height, &bytes_pp, 0);

        Binary::append_image(bytes, ref("Wallpaper"), pixels, width, height, bytes_pp);
    }

    append(bytes, Binary::OBJECT_END);
    
    if (bytes.size != bytes.capacity)
        resize(bytes, bytes.size);  // Shrink the array to free extra memory

    return Bytes { bytes.data, bytes.size };
}

static inline String string_escape_and_copy(const String source)
{
    DynamicArray<char> result = make<DynamicArray<char>>(source.size);

    for (u64 i = 0; i < source.size; i++)
    {
        char ch = source[i];

        switch (ch)
        {
            case '\n':
            {
                append(result, '\\');
                append(result, 'n');
            } break;
            
            case '\t':
            {
                append(result, ' ');    // No need for tabs when packing
            } break;

            // These can be skipped
            case '\b':
            case '\r':
            break;

            default:
            {
                append(result, ch);
            } break;
        }
    }

    // No need to resize cause it's just used for packing

    return String { result.data, result.size };
}

static inline void pack_shader(String shader_name, String shader_path,
                               DynamicArray<String>& builder, DynamicArray<String>& strings_to_be_freed)
{
    append(builder, ref("constexpr char* "));
    append(builder, shader_name);
    append(builder, ref(" = \""));

    String source_raw = file_load_string(shader_path);
    String source_escaped = string_escape_and_copy(source_raw);

    append(builder, source_escaped);

    append(strings_to_be_freed, source_escaped);
    free(source_raw);
    
    append(builder, ref("\";\n"));
}

String pack_shaders()
{
    DynamicArray<String> builder = make<DynamicArray<String>>();
    DynamicArray<String> strings_to_be_freed = make<DynamicArray<String>>();

    append(builder, ref("#pragma once\n\n"));

    pack_shader(ref("ui_quad_vert_shader_source"), ref(ui_quad_vert_shader_path), builder, strings_to_be_freed);
    pack_shader(ref("ui_quad_frag_shader_source"), ref(ui_quad_frag_shader_path), builder, strings_to_be_freed);
    pack_shader(ref("ui_font_vert_shader_source"), ref(ui_font_vert_shader_path), builder, strings_to_be_freed);
    pack_shader(ref("ui_font_frag_shader_source"), ref(ui_font_frag_shader_path), builder, strings_to_be_freed);

    String final = build_string(builder);
    
    for (u64 i = 0; i < strings_to_be_freed.size; i++)
        free(strings_to_be_freed[i]);
    
    free(strings_to_be_freed);
    free(builder);

    return final;
}

}