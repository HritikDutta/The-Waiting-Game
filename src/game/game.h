#pragma once

#include "engine/imgui.h"
#include "containers/bytes.h"
#include "containers/darray.h"
#include "containers/function.h"
#include "core/coroutines.h"

struct GameData;

using GameWindowRenderCallback = Function<void(Coroutine& co, u64 index, Application&, GameData&)>;
using GameShortcutOpenCallback = Function<void(const Application&, GameData&)>;

struct GameProjectDifficulty
{
    f32 loading_speed_base;
    f32 max_interruption_interval;
};

struct GameShortcut
{
    Imgui::Image icon = {};
    String name = {};
    GameShortcutOpenCallback on_open_callback;
    f32 last_click_time = -10.0f;
};

struct GameData
{
    // Presentation Data
    Imgui::Font ui_font = {};
    Imgui::Image desktop_wallpaper = {};
    Vector4 border_color = Vector4 { 0.25f, 0.25f, 0.75f, 1.0f };

    Imgui::Image shortcut_icon_project = {};
    Imgui::Image shortcut_icon_settings = {};
    Imgui::Image shortcut_icon_notes = {};

    // Others
    Imgui::Image shutdown_button_image = {};
    Imgui::Image wallpaper_to_be_deleted = {};

    // Shortcuts
    DynamicArray<GameShortcut> shortcuts;

    // Window Data
    DynamicArray<Coroutine> coroutine_handles;
    DynamicArray<GameWindowRenderCallback> active_game_windows;
    DynamicArray<Vector2> game_window_positions;
    DynamicArray<s32> game_window_ids;  // For Imgui

    // Game Data
    bool started_game;
    bool loading_finished;
    bool showing_project_open_window;
    bool notification_active;
    bool baited;
    bool request_shutdown;
    bool initial_loading;
    bool save_settings;
    f32 loading_progress;

    f32 next_pop_up_time = 0.0f;
    f32 loading_start_time = 0.0f;
    f32 loading_end_time = 0.0f;

    GameProjectDifficulty current_project_difficulty;

    u8* wallpaper_pixels = nullptr;

#ifdef GN_DEBUG
    bool is_debug = false;
    f32 load_speed_multiplier = 0.0f;
#endif // GN_DEBUG
};

const GameProjectDifficulty game_project_difficulty_small  = { 1.0f /  30.0f, 8.0f };
const GameProjectDifficulty game_project_difficulty_medium = { 1.0f /  60.0f, 6.0f };
const GameProjectDifficulty game_project_difficulty_large  = { 1.0f / 120.0f, 5.0f };

void game_init(const Application& app, GameData& data);
void game_reset(GameData& data);

// Shortcut Stuff
void game_render_shortcuts(const Application& app, GameData& data);
void game_shortcut_register(GameData& data, const GameShortcut& shortcut);

// Shortcuts
void game_shortcut_small_project(const Application& app, GameData& data);
void game_shortcut_medium_project(const Application& app, GameData& data);
void game_shortcut_large_project(const Application& app, GameData& data);
void game_shortcut_notes(const Application& app, GameData& data);
void game_shortcut_settings(const Application& app, GameData& data);

// Window Stuff
void game_render_active_windows(Application& app, GameData& data);
void game_post_render(GameData& data);

void game_window_register(GameData& data, GameWindowRenderCallback callback, const Vector2& position);
void game_window_close(GameData& data, u64 index);

// Game Windows
void game_window_main_loading_bar(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_loading_finished(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_project_already_open(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_random_pop_up(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_click_bait(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_notification(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_baited(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_shutdown(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_notes(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_settings(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_initial_loading_screen(Coroutine& co, u64 index, Application& app, GameData& data);
void game_window_shutdown_loading_screen(Coroutine& co, u64 index, Application& app, GameData& data);

// Misc
f32 game_decide_next_interruption_time(const Application& app, const GameData& data);