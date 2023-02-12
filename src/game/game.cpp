#include "game.h"

#include "engine/imgui.h"
#include "containers/darray.h"
#include "containers/function.h"
#include "containers/string.h"
#include "core/coroutines.h"
#include "math/common.h"
#include "platform/platform.h"
#include "serialization/binary.h"
#include "written_content.h"

#include <stb_image.h>

static DynamicArray<u64> game_windows_to_be_closed;
static s32 game_top_most_window_id = -1;
static s32 next_valid_window_id = 1;

constexpr f32 game_font_size_button = 16.0f;
constexpr f32 game_padding_button_horizontal = 10.0f;
constexpr f32 game_padding_button_vertical   = 5.0f;

constexpr f32 game_font_size_ui = 20.0f;
constexpr f32 game_padding_window_horizontal = 10.0f;
constexpr f32 game_padding_window_vertical   = 10.0f;

f32 game_decide_next_interruption_time(const Application& app, const GameData& data)
{
    return app.time + data.current_project_difficulty.max_interruption_interval * Math::random();
}

void game_reset(GameData& data)
{
    data.started_game     = false;
    data.loading_progress = 0.0f;
    data.loading_end_time = 0.0f;
    data.current_project_difficulty = game_project_difficulty_small;
}

void import_raw_assets(GameData& data)
{
    {   // Load Images
        data.shutdown_button_image = texture_load_file(ref("assets/art/power_button.png"), TextureSettings::default());
    }

    {   // Shortcuts
        GameShortcut shortcut;

        Imgui::Image shortcut_icon_project = texture_load_file(ref("assets/art/shortcut_icon_project.png"), TextureSettings::default());

        {   // Small Project
            shortcut.icon = shortcut_icon_project;
            shortcut.name = ref("Small Project");
            shortcut.on_open_callback = game_shortcut_small_project;
            game_shortcut_register(data, shortcut);
        }

        {   // Medium Project
            // shortcut.icon = shortcut_icon_project;
            shortcut.name = ref("Medium Project");
            shortcut.on_open_callback = game_shortcut_medium_project;
            game_shortcut_register(data, shortcut);
        }

        {   // Large Project
            // shortcut.icon = shortcut_icon_project;
            shortcut.name = ref("Large Project");
            shortcut.on_open_callback = game_shortcut_large_project;
            game_shortcut_register(data, shortcut);
        }
        
        {   // Settings App
            shortcut.icon = texture_load_file(ref("assets/art/shortcut_icon_settings.png"), TextureSettings::default());
            shortcut.name = ref("Settings");
            shortcut.on_open_callback = game_shortcut_settings;
            game_shortcut_register(data, shortcut);
        }

        {   // Notes App
            shortcut.icon = texture_load_file(ref("assets/art/shortcut_icon_notes.png"), TextureSettings::default());
            shortcut.name = ref("Quick Notes!");
            shortcut.on_open_callback = game_shortcut_notes;
            game_shortcut_register(data, shortcut);
        }
    }
}

void game_init(const Application& app, GameData& data)
{
    data.shortcuts = make<DynamicArray<GameShortcut>>(5Ui64);

    data.coroutine_handles     = make<DynamicArray<Coroutine>>(10Ui64);
    data.active_game_windows   = make<DynamicArray<GameWindowRenderCallback>>(10Ui64);
    data.game_window_positions = make<DynamicArray<Vector2>>(10Ui64);
    data.game_window_ids       = make<DynamicArray<s32>>(10Ui64);
    game_reset(data);

    game_windows_to_be_closed = make<DynamicArray<u64>>();

    next_valid_window_id = 1;

    data.showing_project_open_window = false;
    data.notification_active = false;
    data.baited = false;
    data.request_shutdown = false;
    data.save_settings = false;

    data.next_pop_up_time = game_decide_next_interruption_time(app, data);

    // Register fake loading screen
    game_window_register(data, game_window_initial_loading_screen, Vector2 {});
    data.initial_loading = true;

    {   // Set up shortcuts
        GameShortcut shortcut;

        {   // Small Project
            shortcut.icon = data.shortcut_icon_project;
            shortcut.name = ref("Small Project");
            shortcut.on_open_callback = game_shortcut_small_project;
            game_shortcut_register(data, shortcut);
        }

        {   // Medium Project
            // shortcut.icon = data.shortcut_icon_project;
            shortcut.name = ref("Medium Project");
            shortcut.on_open_callback = game_shortcut_medium_project;
            game_shortcut_register(data, shortcut);
        }

        {   // Large Project
            // shortcut.icon = data.shortcut_icon_project;
            shortcut.name = ref("Large Project");
            shortcut.on_open_callback = game_shortcut_large_project;
            game_shortcut_register(data, shortcut);
        }
        
        {   // Settings App
            shortcut.icon = data.shortcut_icon_settings;
            shortcut.name = ref("Settings");
            shortcut.on_open_callback = game_shortcut_settings;
            game_shortcut_register(data, shortcut);
        }

        {   // Notes App
            shortcut.icon = data.shortcut_icon_notes;
            shortcut.name = ref("Quick Notes!");
            shortcut.on_open_callback = game_shortcut_notes;
            game_shortcut_register(data, shortcut);
        }
    }
}

void game_window_register(GameData& data, GameWindowRenderCallback callback, const Vector2& position)
{
    append(data.coroutine_handles, Coroutine {});
    append(data.active_game_windows, callback);
    append(data.game_window_positions, position);
    append(data.game_window_ids, next_valid_window_id++);
}

void game_window_close(GameData& data, u64 index)
{
    append(game_windows_to_be_closed, index);
    coroutine_reset(data.coroutine_handles[index]);
}

void game_shortcut_register(GameData& data, const GameShortcut& shortcut)
{
    append(data.shortcuts, shortcut);
}

void game_render_shortcuts(const Application& app, GameData& data)
{
    constexpr f32 icon_padding = 18.0f;
    constexpr f32 icon_size    = 50.0f;
    constexpr f32 icon_rect_padding = 2.0f;

    Imgui::Rect icon_rect;
    icon_rect.top_left = Vector3 { icon_padding, 0.0f, 0.0f };
    icon_rect.size = Vector2 { icon_size, icon_size };

    for (u64 i = 0; i < data.shortcuts.size; i++)
    {
        icon_rect.top_left.y += icon_padding;

        constexpr f32 name_padding   = 5.0f;
        
        f32 name_size_y;
        {   // Shortcut Name
            constexpr f32 name_font_size = 12.0f;

            Vector2 name_size = Imgui::get_rendered_text_size(data.shortcuts[i].name, data.ui_font, name_font_size);
            name_size_y = name_size.y;

            Vector3 top_left = icon_rect.top_left + Vector3 { 0.5f * (icon_size - name_size.x), icon_size + name_padding, 0.0f };
            Imgui::render_text(data.shortcuts[i].name, data.ui_font, top_left + Vector3 { 0.5f, 0.5f, 0.001f }, name_font_size, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
            Imgui::render_text(data.shortcuts[i].name, data.ui_font, top_left, name_font_size);
        }

        {   // Shortcut Icon
            Imgui::render_image(data.shortcuts[i].icon, icon_rect.top_left + Vector3 { icon_rect_padding, icon_rect_padding, 0.001f }, icon_rect.size - Vector2 { 2.0f * icon_rect_padding, 2.0f * icon_rect_padding });
        }

        constexpr f32 double_click_interval = 0.4f;
        const Vector4 default_color = Vector4 {};
        const Vector4 hover_color   = Vector4 { 1.0f, 1.0f, 1.0f, 0.25f };
        if (Imgui::render_button(gen_imgui_id_with_secondary((s32) i), icon_rect,
                                 default_color, hover_color, hover_color))
        {
            f32& last_click_time = data.shortcuts[i].last_click_time;
            if (app.time - last_click_time <= double_click_interval)
            {
                data.shortcuts[i].on_open_callback(app, data);
                last_click_time = -10.0f;
            }
            else
            {
                last_click_time = app.time;
            }
        }

        icon_rect.top_left.y += icon_size + name_padding + name_size_y;
    }
}

static inline void start_loading_game_project(const Application& app, GameData& data, const GameProjectDifficulty& difficulty)
{
    if (!data.started_game)
    {
        game_window_register(data, game_window_main_loading_bar, Vector2 {});
        data.loading_start_time = app.time;
        data.loading_finished   = false;
        data.started_game       = true;
        data.current_project_difficulty = difficulty;
    }
    else if (!data.showing_project_open_window)
    {
        game_window_register(data, game_window_project_already_open, Vector2 {});
    }
}

void game_shortcut_small_project(const Application& app, GameData& data)
{
    start_loading_game_project(app, data, game_project_difficulty_small);
}

void game_shortcut_medium_project(const Application& app, GameData& data)
{
    start_loading_game_project(app, data, game_project_difficulty_medium);
}

void game_shortcut_large_project(const Application& app, GameData& data)
{
    start_loading_game_project(app, data, game_project_difficulty_large);
}

void game_shortcut_notes(const Application& app, GameData& data)
{
    game_window_register(data, game_window_notes, Vector2 {});
}

void game_shortcut_settings(const Application& app, GameData& data)
{
    game_window_register(data, game_window_settings, Vector2 {});
}

struct GameWindowData
{
    Coroutine co;
    GameWindowRenderCallback callback;
    Vector2 position;
    s32 window_id;
};

void game_render_active_windows(Application& app, GameData& data)
{
    // Render windows
    for (u64 i = 0; i < data.active_game_windows.size; i++)
        data.active_game_windows[i](data.coroutine_handles[i], i, app, data);
}

void game_post_render(GameData& data)
{
    if (data.wallpaper_to_be_deleted.id)
    {
        String name = texture_get_name(data.wallpaper_to_be_deleted);
        free(data.wallpaper_to_be_deleted);
        free(name);
    }

    // Close windows
    for (u64 i = 0; i < game_windows_to_be_closed.size; i++)
    {
        remove(data.coroutine_handles, game_windows_to_be_closed[i]);
        remove(data.active_game_windows, game_windows_to_be_closed[i]);
        remove(data.game_window_positions, game_windows_to_be_closed[i]);
        remove(data.game_window_ids, game_windows_to_be_closed[i]);
    }

    // Reorder windows
    if (game_top_most_window_id != -1)
    {
        u64 index_to_move = data.active_game_windows.size;
        GameWindowData window_data;

        // Could optimize this maybe?
        for (u64 i = 0; i < data.active_game_windows.size; i++)
        {
            if (data.game_window_ids[i] == game_top_most_window_id)
            {
                // Copy data to move
                window_data.co = data.coroutine_handles[i];
                window_data.callback = data.active_game_windows[i];
                window_data.position = data.game_window_positions[i];
                window_data.window_id = data.game_window_ids[i];

                index_to_move = i;
                break;
            }
        }

        if (index_to_move != data.active_game_windows.size)
        {
            // Remove from original index
            remove(data.coroutine_handles, index_to_move);
            remove(data.active_game_windows, index_to_move);
            remove(data.game_window_positions, index_to_move);
            remove(data.game_window_ids, index_to_move);

            // Append the data to the window arrays
            append(data.coroutine_handles, window_data.co);
            append(data.active_game_windows, window_data.callback);
            append(data.game_window_positions, window_data.position);
            append(data.game_window_ids, window_data.window_id);
        }
    }

    game_top_most_window_id = -1;
    clear(game_windows_to_be_closed);
}

static inline void game_window_bring_to_front(GameData& data, u64 index)
{
    game_top_most_window_id = data.game_window_ids[index];
}

// Returns true if the X button was pressed
static bool game_window_render_background(const Imgui::Rect& window_rect, const String window_name, GameData& data, u64 index,
                                          const Vector4& border_color_active, const Vector4& background_color = Vector4 { 0.65f, 0.65f, 0.65f, 1.0f })
{
    bool pressed_x_button;

    {   // Border
        constexpr f32 border_thickness = 4.0f;
        constexpr f32 window_name_height = game_font_size_ui + border_thickness + border_thickness;

        Imgui::Rect border_rect;
        border_rect.size = window_rect.size + Vector2 { 2 * border_thickness, 2 * border_thickness + window_name_height };
        border_rect.top_left = window_rect.top_left - Vector3 { border_thickness, border_thickness + window_name_height, -0.001f };

        {   // Window Border
            const Vector4 border_color_inactive = Vector4 { 0.45f, 0.45f, 0.45f, 1.0f };

            const Vector4& border_color = (index == data.active_game_windows.size - 1) ? border_color_active : border_color_inactive;
            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), border_rect, border_color, border_color, border_color))
                game_window_bring_to_front(data, index);
        }

        {   // Shadow
            Imgui::Rect shadow_rect = border_rect;
            shadow_rect.top_left += Vector3 { 5.0f, 5.0f, 0.0001f };
            Imgui::render_rect(shadow_rect, Vector4 { 0.0f, 0.0f, 0.0f, 0.25f });
        }

        {   // Window Name
            Vector3 top_left = border_rect.top_left + Vector3 { 2 * border_thickness, border_thickness, -0.001f };
            Imgui::render_text(window_name, data.ui_font, top_left, game_font_size_ui);
        }

        {   // X Button
            String text  = ref("X");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = border_rect.top_left + Vector3 { border_rect.size.x - (rect.size.x + border_thickness), 0.75f * border_thickness, -0.001f };

            const Vector4 button_color_default = Vector4 { 0.75f, 0.0f, 0.0f, 1.0f };
            const Vector4 button_color_hover   = Vector4 { 0.9f, 0.0f, 0.0f, 1.0f };
            const Vector4 button_color_pressed = Vector4 { 0.6f, 0.0f, 0.0f, 1.0f };

            pressed_x_button = Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect, button_color_default, button_color_hover, button_color_pressed);
            if (pressed_x_button)
                game_window_close(data, index);

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button);
        }
    }

    Imgui::render_rect(window_rect, background_color);

    return pressed_x_button;
}

static void game_window_render_background_fake(const Imgui::Rect& window_rect, const String window_name, GameData& data,
                                               const Vector4& border_color, const Vector4& background_color = Vector4 { 0.65f, 0.65f, 0.65f, 1.0f })
{
    {   // Border
        constexpr f32 border_thickness = 2.0f;
        constexpr f32 font_size = 0.5f * game_font_size_ui;
        constexpr f32 window_name_height = font_size + border_thickness + border_thickness;

        Imgui::Rect border_rect;
        border_rect.size = window_rect.size + Vector2 { 2 * border_thickness, 2 * border_thickness + window_name_height };
        border_rect.top_left = window_rect.top_left - Vector3 { border_thickness, border_thickness + window_name_height, -0.001f };

        {   // Window Border
            Imgui::render_rect(border_rect, border_color);
        }

        {   // Shadow
            Imgui::Rect shadow_rect = border_rect;
            shadow_rect.top_left += Vector3 { 5.0f, 5.0f, 0.0001f };
            Imgui::render_rect(shadow_rect, Vector4 { 0.0f, 0.0f, 0.0f, 0.25f });
        }

        {   // Window Name
            Vector3 top_left = border_rect.top_left + Vector3 { 2 * border_thickness, border_thickness, -0.001f };
            Imgui::render_text(window_name, data.ui_font, top_left, font_size);
        }

        {   // X Button
            String text  = ref("X");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, font_size);

            Imgui::Rect rect;
            rect.size = size + Vector2 { game_padding_button_horizontal, 0.75f * game_padding_button_vertical };
            rect.top_left = border_rect.top_left + Vector3 { border_rect.size.x - (rect.size.x + border_thickness), 0.75f * 0.5f * border_thickness, -0.001f };

            const Vector4 button_color = Vector4 { 0.75f, 0.0f, 0.0f, 1.0f };
            Imgui::render_rect(rect, button_color);

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + 0.5f * game_padding_button_horizontal, rect.top_left.y + 0.5f * game_padding_button_vertical, rect.top_left.z - 0.001f }, font_size);
        }
    }

    Imgui::render_rect(window_rect, background_color);
}

void game_window_main_loading_bar(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    bool& closed = coroutine_stack_variable<bool>(co);

    coroutine_start(co);

    while (!data.loading_finished)
    {
        Imgui::Rect window_rect;
        window_rect.size = Vector2 { 620, 100 };
        window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

        closed = game_window_render_background(window_rect, ref("Immunity Loader"), data, index, data.border_color);

        {   // Content
            {   // Body Text
                Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };

                char buffer[128];
                sprintf(buffer, "Loading Project... (%.1fs)", app.time - data.loading_start_time);

                String text = ref(buffer);
                Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
            }

            {   // Loading Bar Background
                Imgui::Rect rect;
                rect.size = Vector2 { window_rect.size.x - (2 * game_padding_window_horizontal), 40.0f };
                rect.top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };
                Imgui::render_rect(rect, Vector4 { 1.0f, 1.0f, 1.0f, 1.0f });
            }

            {   // Loading Bar Progress
                Imgui::Rect rect;
                rect.size = Vector2 { data.loading_progress * window_rect.size.x - (2 * game_padding_window_horizontal), 40.0f };
                rect.top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.002f };
                Imgui::render_rect(rect, Vector4 { 0.0f, 1.0f, 0.0f, 1.0f });
            }
        }

        if (closed)
            game_reset(data);

        coroutine_yield(co);
    }

    if (!closed)
    {
        game_window_close(data, index);
        game_window_register(data, game_window_loading_finished, Vector2 {});
    }

    coroutine_yield(co);

    coroutine_end(co);
}

void game_window_loading_finished(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    bool& closed = coroutine_stack_variable<bool>(co);

    coroutine_start(co);

    closed = false;
    while (!closed)
    {
        Imgui::Rect window_rect;
        window_rect.size = Vector2 { 500, 150 };
        window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

        closed = game_window_render_background(window_rect, ref("Immunity Loader"), data, index, data.border_color);

        {   // Content
            {   // Body Text
                Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
                
                char buffer[128];
                sprintf(buffer, "Project loaded successfully!\n(Time taken %.1fs)", data.loading_end_time - data.loading_start_time);

                String text = ref(buffer);
                Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
            }

            {   // Okay Button
                String text  = ref("Ok");
                Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                Imgui::Rect rect;
                rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
                rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

                if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
                {
                    game_window_close(data, index);
                    closed = true;
                }

                Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
            }
        }

        if (closed)
            game_reset(data);

        coroutine_yield(co);
    }

    coroutine_end(co);
}

void game_window_project_already_open(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    bool& closed = coroutine_stack_variable<bool>(co);

    coroutine_start(co);

    data.showing_project_open_window = true;

    closed = false;
    while (!closed)
    {
        Imgui::Rect window_rect;
        window_rect.size = Vector2 { 300, 100 };
        window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

        closed = game_window_render_background(window_rect, ref("Immunity Loader"), data, index, data.border_color);

        {   // Content
            {   // Body Text
                Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
                String text = ref("Project already open!");
                Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
            }

            {   // Okay Button

                String text  = ref("Ok");
                Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                Imgui::Rect rect;
                rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
                rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

                if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
                {
                    game_window_close(data, index);
                    closed = true;
                }

                Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
            }
        }

        if (closed)
            data.showing_project_open_window = false;

        coroutine_yield(co);
    }

    coroutine_end(co);
}

void game_window_random_pop_up(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& offset = coroutine_stack_variable<Vector2>(co);
    f32& start_time  = coroutine_stack_variable<f32>(co);

    constexpr f32 animation_length = 0.5f;

    coroutine_start(co);

    start_time = app.time;
    while (app.time - start_time <= animation_length)
    {
        offset.x = 5.0f * Math::random();
        offset.y = 5.0f * Math::random();

        coroutine_yield(co);
    }

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { 300, 150 };
    window_rect.top_left = Vector3 { data.game_window_positions[index].x + offset.x, data.game_window_positions[index].y + offset.y, -layer };

    bool closed = game_window_render_background(window_rect, ref("H0T $1NGL3S!"), data, index, data.border_color);

    {   // Content
        {   // Body Text
            Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
            String text = ref("Roma wants to chat with you...");
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
        }

        {   // Close Button
            String text  = ref("Close");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect) && !closed)
                game_window_close(data, index);

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }
    }
}

void game_window_click_bait(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& offset = coroutine_stack_variable<Vector2>(co);
    f32& start_time = coroutine_stack_variable<f32>(co);

    constexpr f32 animation_length = 0.5f;

    coroutine_start(co);

    start_time = app.time;
    while (app.time - start_time <= animation_length)
    {
        offset.x = 5.0f * Math::random();
        offset.y = 5.0f * Math::random();

        coroutine_yield(co);
    }

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { 300, 150 };
    window_rect.top_left = Vector3 { data.game_window_positions[index].x + offset.x, data.game_window_positions[index].y + offset.y, -layer };

    bool closed = game_window_render_background(window_rect, ref("New Offer Alerts!"), data, index, data.border_color);

    {   // Content
        {   // Body Text
            Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
            String text = ref("New Offer! Click open to\nopen in new window...");
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
        }

        {   // Open Button
            String text  = ref("Open");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
            {
                game_window_close(data, index);

                if (!data.baited)
                    game_window_register(data, game_window_baited, Vector2 {});
            }

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }

        {   // Close Button
            String text  = ref("Close");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect) && !closed)
                game_window_close(data, index);

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }
    }
}

void game_window_notification(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    f32& x_offset = coroutine_stack_variable<f32>(co);
    f32& t = coroutine_stack_variable<f32>(co);

    constexpr f32 notification_x_size = 300.0f;
    constexpr f32 notification_move_speed = 10.0f;

    coroutine_start(co);

    data.notification_active = true;
    t = 0.0f;
    while (t <= 1.0f)
    {
        x_offset = lerp(0.0f, notification_x_size, t);
        t = clamp(t + notification_move_speed * app.delta_time, 0.0f, 1.0f);
        coroutine_yield(co);
    }

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { notification_x_size, 150 };
    window_rect.top_left = Vector3 { app.window.ref_width - x_offset - 4.0f, app.window.ref_height - window_rect.size.y - 4.0f, -layer };

    bool closed = game_window_render_background(window_rect, ref("Flack"), data, index, data.border_color);

    {   // Content
        {   // Body Text
            Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
            String text = ref("Message from Chaudhary!!!");
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
        }

        {   // Okay Button
            String text  = ref("Ok");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
            {
                game_window_close(data, index);
                closed = false;
            }

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }

        if (closed)
            data.notification_active = false;
    }
}

void game_window_baited(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& window_size_animated = coroutine_stack_variable<Vector2>(co);
    f32& t = coroutine_stack_variable<f32>(co);

    constexpr f32 window_open_speed = 10.0f;

    coroutine_start(co);

    data.baited = true;
    t = 0.0f;
    while (t <= 1.0f)
    {
        window_size_animated = lerp(Vector2 {}, Vector2 { app.window.ref_width * 0.8f, app.window.ref_height * 0.9f }, t);
        t = clamp(t + window_open_speed * app.delta_time, 0.0f, 1.0f);
        coroutine_yield(co);
    }

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = window_size_animated;
    window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

    bool closed = game_window_render_background(window_rect, ref("Foogle Rome"), data, index, data.border_color, Vector4 { 1.0f, 1.0f, 1.0f, 1.0f });
    const Vector4 black = Vector4 { 0.0f, 0.0f, 0.0f, 1.0f };

    constexpr f32 line_distance = 3.0f;

    // Content
    if (t >= 0.9f)
    {
        f32 y = game_padding_window_vertical;

        constexpr f32 font_size_heading = 30.0f;

        {   // Heading 1
            const String text = ref(baited_window_heading_1);
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, font_size_heading);

            Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), y , -0.001f };
            Imgui::render_text(text, data.ui_font, top_left, font_size_heading, black);

            y += size.y + line_distance;
        }

        {   // Heading 2
            const String text = ref(baited_window_heading_2);
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, font_size_heading);

            Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), y , -0.001f };
            Imgui::render_text(text, data.ui_font, top_left, font_size_heading, black);

            y += size.y + line_distance;
        }

        {   // Heading 3
            const String text = ref(baited_window_heading_3);
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, font_size_heading);

            Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), y , -0.001f };
            Imgui::render_text(text, data.ui_font, top_left, font_size_heading, black);

            y += size.y + line_distance;
        }

        y += 20.0f;

        {   // Body Text
            const String text = ref(baited_window_text, baited_window_text_size);
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_ui);

            Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), y , -0.001f };
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, black);

            y += size.y + line_distance;
        }

        if (closed)
            data.baited = false;
    }
}

void game_window_shutdown(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& offset = coroutine_stack_variable<Vector2>(co);
    f32& start_time = coroutine_stack_variable<f32>(co);

    constexpr f32 animation_length = 0.5f;

    coroutine_start(co);

    start_time = app.time;
    while (app.time - start_time <= animation_length)
    {
        offset.x = 5.0f * Math::random();
        offset.y = 5.0f * Math::random();

        coroutine_yield(co);
    }

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { 350, 100 };
    window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x) + offset.x, 0.5f * (app.window.ref_height - window_rect.size.y) + offset.y, -layer };

    bool closed = game_window_render_background(window_rect, ref("Shutdown"), data, index, data.border_color);

    {   // Content
        {   // Body Text
            Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
            String text = ref("Are you sure you want to shutdown?");
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
        }

        {   // Open Button
            String text  = ref("Yes");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - (rect.size.x + game_padding_window_horizontal), window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
            {
                game_window_close(data, index);
                game_window_register(data, game_window_shutdown_loading_screen, Vector2 {});
            }

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }

        {   // Close Button
            String text  = ref("No");
            Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

            Imgui::Rect rect;
            rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
            rect.top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, window_rect.size.y - rect.size.y - game_padding_window_vertical, -0.001f };

            if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect) && !closed)
                game_window_close(data, index);

            Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});
        }
    }
}

void game_window_notes(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& window_size_animated = coroutine_stack_variable<Vector2>(co);
    f32& t = coroutine_stack_variable<f32>(co);

    constexpr f32 window_open_speed = 10.0f;
    const Vector2 window_size = Vector2 { 620.0f, 300.0f };

    coroutine_start(co);

    t = 0.0f;
    while (t <= 1.0f)
    {
        window_size_animated = lerp(Vector2 {}, window_size, t);
        t = clamp(t + window_open_speed * app.delta_time, 0.0f, 1.0f);
        coroutine_yield(co);
    }

    coroutine_end(co);
    
    Imgui::Rect window_rect;
    window_rect.size = window_size_animated;
    window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

    const Vector4 background_color = Vector4 { 0.95f, 0.95f, 0.35f, 1.0f };
    const Vector4 border_color = Vector4 { 1.0f, 0.7254902120f, 0.0f, 1.0f };
    bool closed = game_window_render_background(window_rect, ref("Quick Notes!"), data, index, border_color, background_color);

    // Content
    if (t >= 0.9f)
    {
        {   // Body Text
            Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
            const String text = ref(notes_text, notes_text_size);
            Imgui::render_text(text, data.ui_font, top_left, game_font_size_ui, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
        }
    }
}

void game_window_settings(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32 layer = (f32) index / (f32) data.active_game_windows.size;

    Vector2& window_size_animated = coroutine_stack_variable<Vector2>(co);
    f32& t = coroutine_stack_variable<f32>(co);

    Imgui::Image& wallpaper_selected = coroutine_stack_variable<Imgui::Image>(co);
    Vector4& border_color_selected = coroutine_stack_variable<Vector4>(co);
    WindowStyle&  window_style_selected = coroutine_stack_variable<WindowStyle>(co);

    u8*& temp_wallpaper_pixels = coroutine_stack_variable<u8*>(co);

    bool& wallpaper_dirty    = coroutine_stack_variable<bool>(co);
    bool& border_color_dirty = coroutine_stack_variable<bool>(co);
    bool& window_style_dirty = coroutine_stack_variable<bool>(co);

    constexpr f32 window_open_speed = 10.0f;

    constexpr f32 window_size_x = 2.0f * game_padding_window_horizontal + 356.0f + 6.0f * (25.0f + game_padding_window_horizontal);
    const Vector2 window_size = Vector2 { window_size_x, 250.0f };

    coroutine_start(co);

    wallpaper_selected = data.desktop_wallpaper;
    border_color_selected = data.border_color;
    window_style_selected = app.window.style;

    wallpaper_dirty = border_color_dirty = window_style_dirty = false;

    temp_wallpaper_pixels = nullptr;

    t = 0.0f;
    while (t <= 1.0f)
    {
        window_size_animated = lerp(Vector2 {}, window_size, t);
        t = clamp(t + window_open_speed * app.delta_time, 0.0f, 1.0f);
        coroutine_yield(co);
    }

    coroutine_end(co);
    
    Imgui::Rect window_rect;
    window_rect.size = window_size_animated;
    window_rect.top_left = Vector3 { 0.5f * (app.window.ref_width - window_rect.size.x), 0.5f * (app.window.ref_height - window_rect.size.y), -layer };

    bool closed = game_window_render_background(window_rect, ref("Settings"), data, index, data.border_color);

    // Content
    if (t >= 0.9f)
    {
        f32 display_area_left;

        {   // Display Area
            Imgui::Rect wallpaper_rect;

            {   // Current Wallpaper
                wallpaper_rect.size = Vector2 { 356.0f, 200.0f };
                wallpaper_rect.top_left = window_rect.top_left + Vector3 { window_rect.size.x - wallpaper_rect.size.x - game_padding_window_horizontal, game_padding_window_vertical, -0.001f };
                Imgui::render_rect(wallpaper_rect, app.clear_color); // App background color for transparent wallpapers 

                wallpaper_rect.top_left.z -= 0.002f;
                Imgui::render_image(wallpaper_selected, wallpaper_rect.top_left, wallpaper_rect.size);
            }

            {   // Example Window
                Imgui::Rect example_window_rect;
                example_window_rect.size = Vector2 { 192.0f, 108.0f };
                example_window_rect.top_left = wallpaper_rect.top_left + Vector3 { 2.0f * game_padding_window_horizontal, 3.0f * game_padding_window_vertical, -0.004f };
                game_window_render_background_fake(example_window_rect, ref("Example"), data, border_color_selected);
            }

            display_area_left = wallpaper_rect.top_left.x - window_rect.top_left.x;
        }

        {   // Options
            f32 y = game_padding_window_vertical;

            {   // Choose Wallpaper
                f32 layout_y = 0.0f;

                {   // Label
                    String text = ref("Wallpaper:");
                    Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                    Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, y + game_padding_button_vertical, -0.001f };
                    Imgui::render_text(text, data.ui_font, top_left, game_font_size_button, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
                }

                {   // Button
                    String text = ref("Choose");
                    Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                    Imgui::Rect rect;
                    rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
                    rect.top_left = window_rect.top_left + Vector3 { display_area_left - size.x - game_padding_window_horizontal - 2 * game_padding_button_horizontal, y, -0.001f };

                    if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
                    {
                        game_window_bring_to_front(data, index);

                        constexpr char filter[] = "Images (*.PNG, *.JPG, *.JPEG)\0*.PNG;*.JPG;*.JPEG\0PNG (*.PNG)\0*.PNG\0JPEG (*.JPG, *.JPEG)\0*.JPG;*.JPEG\0\0";
                        char filename[512] = "";
                        if (platform_dialogue_open_file(filter, filename, 512))
                        {
                            // Delete temporary wallpaper only if it wasn't the desktop wallpaper
                            if (wallpaper_selected.id != data.desktop_wallpaper.id)
                            {
                                String name = texture_get_name(wallpaper_selected);
                                free(wallpaper_selected);
                                free(name);
                            }

                            {   // Load new wallpaper
                                if (temp_wallpaper_pixels)
                                    free(temp_wallpaper_pixels);

                                stbi_set_flip_vertically_on_load(true);

                                s32 width, height, bytes_pp;
                                u8* pixels = stbi_load(filename, &width, &height, &bytes_pp, 4);

                                wallpaper_selected = texture_load_pixels(make<String>((const char*) filename), pixels, width, height, 4, TextureSettings::default());
                                wallpaper_dirty = true;
                            }

                        }
                    }

                    Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});

                    layout_y = rect.size.y;
                }

                y += layout_y + game_padding_window_vertical;
            }

            {   // Window Colors
                {   // Label
                    String text = ref("Window Color:");
                    Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                    Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, y + game_padding_button_vertical, -0.001f };
                    Imgui::render_text(text, data.ui_font, top_left, game_font_size_button, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });

                    y += size.y + game_padding_window_vertical;
                }

                {   // Color Options
                    static const Vector4 color_options[] = {
                        Vector4 { 1.000000000f, 0.7254902120f, 0.0000000000f, 1.0f },
                        Vector4 { 0.968627453f, 0.3882353010f, 0.0470588244f, 1.0f },
                        Vector4 { 1.000000000f, 0.2627451120f, 0.2627451120f, 1.0f },
                        Vector4 { 0.909803927f, 0.0666666701f, 0.1372549090f, 1.0f },
                        Vector4 { 0.890196085f, 0.0000000000f, 0.5490196350f, 1.0f },
                        Vector4 { 0.760784328f, 0.2235294130f, 0.7019608020f, 1.0f },
                        Vector4 { 0.603921592f, 0.0000000000f, 0.5372549300f, 1.0f },
                        Vector4 { 0.454901963f, 0.3019607960f, 0.6627451180f, 1.0f },
                        Vector4 { 0.419607848f, 0.4117647110f, 0.8392156960f, 1.0f },
                        Vector4 { 0.000000000f, 0.4705882370f, 0.8431372640f, 1.0f },
                        Vector4 { 0.250000000f, 0.2500000000f, 0.7500000000f, 1.0f },
                        Vector4 { 0.000000000f, 0.7176470760f, 0.7647058960f, 1.0f },
                        Vector4 { 0.000000000f, 0.6980392340f, 0.5803921820f, 1.0f },
                        Vector4 { 0.000000000f, 0.8000000120f, 0.4156862800f, 1.0f },
                        Vector4 { 0.0627451017f, 0.537254930f, 0.2431372550f, 1.0f },
                        Vector4 { 0.286274523f, 0.5098039510f, 0.0196078438f, 1.0f },
                        Vector4 { 0.517647088f, 0.4588235320f, 0.2705882490f, 1.0f },
                        Vector4 { 0.364705890f, 0.3529411850f, 0.3450980480f, 1.0f },
                    };

                    constexpr s32 row = 3;
                    constexpr s32 columns = (sizeof(color_options) / sizeof(Vector4)) / row;

                    Imgui::Rect color_button_rect;
                    color_button_rect.size = Vector2 { 25.0f, 25.0f };
                    color_button_rect.top_left.z = window_rect.top_left.z - 0.001f;

                    for (s32 i = 0; i < row; i++)
                    {
                        color_button_rect.top_left.x = window_rect.top_left.x + game_padding_window_horizontal;
                        color_button_rect.top_left.y = window_rect.top_left.y + y;

                        for (s32 j = 0; j < columns; j++)
                        {
                            const s32 color_index = i * columns + j;

                            if (border_color_selected == color_options[color_index])
                            {
                                Imgui::render_rect(color_button_rect, Vector4 { 1.0f, 1.0f, 0.0f, 1.0f });
                            }
                            else if (Imgui::render_button(gen_imgui_id_with_secondary(color_index), color_button_rect,
                                                          Vector4 { 0.0f, 0.0f, 0.0f, 1.0f },
                                                          Vector4 { 1.0f, 1.0f, 1.0f, 1.0f },
                                                          Vector4 { 0.3f, 0.3f, 0.3f, 1.0f }))
                            {
                                game_window_bring_to_front(data, index);

                                border_color_selected = color_options[color_index];
                                border_color_dirty    = true;
                            }
                            
                            // Color
                            constexpr f32 border_width = 2.5f;
                            Imgui::Rect rect = color_button_rect;
                            rect.size -= Vector2 { 2.0f * border_width, 2.0f * border_width };
                            rect.top_left += Vector3 { border_width, border_width, -0.001f };
                            Imgui::render_rect(rect, color_options[color_index]);

                            color_button_rect.top_left.x += color_button_rect.size.x + game_padding_window_horizontal;
                        }

                        y += color_button_rect.size.y + game_padding_window_vertical;
                    }
                }
            }

            {   // Game Border Style
                f32 layout_y = 0.0f;

                {   // Label
                    String text = ref("Game Border:");
                    Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                    Vector3 top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, y + game_padding_button_vertical, -0.001f };
                    Imgui::render_text(text, data.ui_font, top_left, game_font_size_button, Vector4 { 0.0f, 0.0f, 0.0f, 1.0f });
                }

                {   // Button
                    String text = window_style_name(window_style_selected);
                    Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                    Imgui::Rect rect;
                    rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
                    rect.top_left = window_rect.top_left + Vector3 { display_area_left - size.x - game_padding_window_horizontal - 2 * game_padding_button_horizontal, y, -0.001f };
                    // rect.top_left = window_rect.top_left + Vector3 { x, y, -0.001f };

                    if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
                    {
                        game_window_bring_to_front(data, index);

                        window_style_selected = (WindowStyle) ((((u32) window_style_selected) + 1) % (u32) WindowStyle::NUM_STYLES);
                        window_style_dirty = true;
                    }

                    Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});

                    layout_y = rect.size.y;
                }

                y += layout_y + game_padding_window_vertical;
            }

            {   // Apply changes
                String text = ref("Apply");
                Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, game_font_size_button);

                Imgui::Rect rect;
                rect.size = size + Vector2 { 2 * game_padding_button_horizontal, 2 * game_padding_button_vertical };
                rect.top_left = window_rect.top_left + Vector3 { game_padding_window_horizontal, y, -0.001f };

                if (Imgui::render_button(gen_imgui_id_with_secondary(data.game_window_ids[index]), rect))
                {
                    game_window_bring_to_front(data, index);

                    data.save_settings = wallpaper_dirty || border_color_dirty || window_style_dirty;

                    if (wallpaper_dirty)
                    {
                        data.wallpaper_to_be_deleted = data.desktop_wallpaper;
                        data.desktop_wallpaper = wallpaper_selected;

                        {   // Load new wallpaper pixels
                            stbi_image_free(data.wallpaper_pixels);
                            
                            s32 width, height, bytes_pp;
                            const String name = texture_get_name(wallpaper_selected);
                            data.wallpaper_pixels = stbi_load(name.data, &width, &height, &bytes_pp, 4);
                        }

                        wallpaper_dirty = false;
                    }

                    if (border_color_dirty)
                    {
                        data.border_color = border_color_selected;
                        border_color_dirty = false;
                    }

                    if (window_style_dirty)
                    {
                        application_set_window_style(app, window_style_selected);
                        window_style_dirty = false;
                    }
                }

                Imgui::render_text(text, data.ui_font, Vector3 { rect.top_left.x + game_padding_button_horizontal, rect.top_left.y + game_padding_button_vertical, rect.top_left.z - 0.001f }, game_font_size_button, Vector4 {0.0, 0.0f, 0.0f, 1.0f});

                y += rect.size.y + game_padding_window_vertical;
            }
        }
    }
}

static inline f32 ease_in_out_function(f32 t)
{
    f32 bezier = t * t * (3.0f - 2.0f * t);
    return bezier * bezier;
}

void game_window_initial_loading_screen(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32& t = coroutine_stack_variable<f32>(co);

    constexpr f32 loading_speed = 1.0f / 10.0f;
    
    coroutine_start(co);

    t = 0.0f;
    while (t <= 1.0f)
    {
        t += loading_speed * app.delta_time;
        coroutine_yield(co);
    }

    data.initial_loading = false;
    game_window_close(data, index);
    data.next_pop_up_time = game_decide_next_interruption_time(app, data);

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { (f32) app.window.ref_width, (f32) app.window.ref_height };
    window_rect.top_left = Vector3 { 0.0f, 0.0f, -0.999f };

    Imgui::render_overlap_rect(gen_imgui_id(), window_rect, data.border_color);

    f32 transition_t  = ease_in_out_function(clamp(t * 10.0f - 2.0f, 0.0f, 1.0f));
    f32 text_offset_y = transition_t * -35.0f;
    f32 text_alpha    = min(t * 5.0f, 1.0f);
    f32 loading_alpha = transition_t;

    f32 y = 0;

    {   // Text
        String text = ref("Welcome");
        Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, 48.0f);
        Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), 0.5f * (window_rect.size.y - size.y) + text_offset_y, -0.0001f };

        Imgui::render_text(text, data.ui_font, top_left, 48.0f, Vector4 { 1.0f, 1.0f, 1.0f, text_alpha });

        y = top_left.y + size.y + 15.0f;
    }

    {   // Loading Bar
        constexpr f32 loading_bar_size_x = 400.0f;
        constexpr f32 loading_bar_size_y = 40.0f;

        {   // Loading Bar Background
            Imgui::Rect rect;
            rect.size = Vector2 { loading_bar_size_x, loading_bar_size_y };
            rect.top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - loading_bar_size_x), y, -0.0001f };
            Imgui::render_rect(rect, Vector4 { 1.0f, 1.0f, 1.0f, loading_alpha });
        }

        {   // Loading Bar Progress
            f32 modified_t = ease_in_out_function(t);

            Imgui::Rect rect;
            rect.size = Vector2 { modified_t * loading_bar_size_x, loading_bar_size_y };
            rect.top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - loading_bar_size_x), y, -0.0002f };
            Imgui::render_rect(rect, Vector4 { 0.0f, 1.0f, 0.0f, loading_alpha });
        }
    }
}

void game_window_shutdown_loading_screen(Coroutine& co, u64 index, Application& app, GameData& data)
{
    f32& t = coroutine_stack_variable<f32>(co);

    constexpr f32 loading_speed = 1.0f / 15.0f;
    
    coroutine_start(co);

    t = 0.0f;
    while (t <= 1.0f)
    {
        t += loading_speed * app.delta_time;
        coroutine_yield(co);
    }

    data.request_shutdown = true;

    coroutine_end(co);

    Imgui::Rect window_rect;
    window_rect.size = Vector2 { (f32) app.window.ref_width, (f32) app.window.ref_height };
    window_rect.top_left = Vector3 { 0.0f, 0.0f, -0.999f };

    Imgui::render_overlap_rect(gen_imgui_id(), window_rect, data.border_color);

    f32 y = 0;

    {   // Text
        String text = ref("Shutting down...");
        Vector2 size = Imgui::get_rendered_text_size(text, data.ui_font, 48.0f);
        Vector3 top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - size.x), 0.5f * (window_rect.size.y - size.y) - 35.0f, -0.0001f };

        Imgui::render_text(text, data.ui_font, top_left, 48.0f, Vector4 { 1.0f, 1.0f, 1.0f, 1.0f });

        y = top_left.y + size.y + 15.0f;
    }

    {   // Loading Bar
        constexpr f32 loading_bar_size_x = 400.0f;
        constexpr f32 loading_bar_size_y = 40.0f;

        {   // Loading Bar Background
            Imgui::Rect rect;
            rect.size = Vector2 { loading_bar_size_x, loading_bar_size_y };
            rect.top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - loading_bar_size_x), y, -0.0001f };
            Imgui::render_rect(rect, Vector4 { 1.0f, 1.0f, 1.0f, 1.0f });
        }

        {   // Loading Bar Progress
            f32 modified_t = ease_in_out_function(t);
            modified_t = (modified_t > 0.65f && modified_t < 0.8f) ? 0.65f : modified_t;  // Fake hiccup in loading

            Imgui::Rect rect;
            rect.size = Vector2 { modified_t * loading_bar_size_x, loading_bar_size_y };
            rect.top_left = window_rect.top_left + Vector3 { 0.5f * (window_rect.size.x - loading_bar_size_x), y, -0.0002f };
            Imgui::render_rect(rect, Vector4 { 0.0f, 1.0f, 0.0f, 1.0f });
        }
    }
}