#include "game_loader.h"

#include "application/application.h"
#include "core/types.h"
#include "core/logger.h"
#include "containers/bytes.h"
#include "serialization/binary.h"
#include "engine/imgui.h"
#include "game.h"

void game_load_assets(const Bytes& bytes, GameData& data)
{
    u64 offset = 1; // Skip object start byte

    {   // Font
        Bytes font_bytes = Binary::get<Bytes>(bytes, offset);
        data.ui_font = Imgui::font_load_from_bytes(font_bytes);
    }

    {   // Power Button
        s32 width    = Binary::get<s32>(bytes, offset);
        s32 height   = Binary::get<s32>(bytes, offset);
        s32 bytes_pp = Binary::get<s32>(bytes, offset);

        String name = Binary::get<String>(bytes, offset);

        Bytes pixels = Binary::get<Bytes>(bytes, offset);
        data.shutdown_button_image = texture_load_pixels(copy(name), pixels.data, width, height, bytes_pp, TextureSettings::default());
    }

    {   // Shortcut Icon Project
        s32 width    = Binary::get<s32>(bytes, offset);
        s32 height   = Binary::get<s32>(bytes, offset);
        s32 bytes_pp = Binary::get<s32>(bytes, offset);

        String name = Binary::get<String>(bytes, offset);

        Bytes pixels = Binary::get<Bytes>(bytes, offset);
        data.shortcut_icon_project = texture_load_pixels(copy(name), pixels.data, width, height, bytes_pp, TextureSettings::default());
    }
    
    {   // Shortcut Icon Settings
        s32 width    = Binary::get<s32>(bytes, offset);
        s32 height   = Binary::get<s32>(bytes, offset);
        s32 bytes_pp = Binary::get<s32>(bytes, offset);

        String name = Binary::get<String>(bytes, offset);

        Bytes pixels = Binary::get<Bytes>(bytes, offset);
        data.shortcut_icon_settings = texture_load_pixels(copy(name), pixels.data, width, height, bytes_pp, TextureSettings::default());
    }
    
    {   // Shortcut Icon Notes
        s32 width    = Binary::get<s32>(bytes, offset);
        s32 height   = Binary::get<s32>(bytes, offset);
        s32 bytes_pp = Binary::get<s32>(bytes, offset);

        String name = Binary::get<String>(bytes, offset);

        Bytes pixels = Binary::get<Bytes>(bytes, offset);
        data.shortcut_icon_notes = texture_load_pixels(copy(name), pixels.data, width, height, bytes_pp, TextureSettings::default());
    }

    gn_assert_with_message(offset == bytes.size - 1, "For some reason there's extra data in the settings bytes! (file size: %, stopped parsing at: %)", bytes.size, offset);
}

void game_load_settings(const Bytes& bytes, Application& app, GameData& data)
{
    u64 offset = 1; // Skip object start byte

    {   // Game Border Style
        WindowStyle style = (WindowStyle) Binary::get<u32>(bytes, offset);
        application_set_window_style(app, style);
    }

    {   // Window Color
        data.border_color.r = Binary::get<f32>(bytes, offset);
        data.border_color.g = Binary::get<f32>(bytes, offset);
        data.border_color.b = Binary::get<f32>(bytes, offset);
        data.border_color.a = 1.0f;
    }

    {   // Wallpaper
        s32 width    = Binary::get<s32>(bytes, offset);
        s32 height   = Binary::get<s32>(bytes, offset);
        s32 bytes_pp = Binary::get<s32>(bytes, offset);

        String name = Binary::get<String>(bytes, offset);

        Bytes pixels = Binary::get<Bytes>(bytes, offset);

        data.desktop_wallpaper = texture_load_pixels(copy(name), pixels.data, width, height, bytes_pp, TextureSettings::default());

        data.wallpaper_pixels = (u8*) platform_reallocate(data.wallpaper_pixels, width * height * bytes_pp);
        gn_assert_with_message(data.wallpaper_pixels, "Couldn't reallocate data for storing wallpaper pixels");
        platform_copy_memory(data.wallpaper_pixels, pixels.data, pixels.size);
    }

    gn_assert_with_message(offset == bytes.size - 1, "For some reason there's extra data in the settings bytes! (file size: %, stopped parsing at: %)", bytes.size, offset);
}