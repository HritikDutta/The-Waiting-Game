#include "application/application.h"
#include "containers/string.h"
#include "core/logger.h"
#include "core/utils.h"
#include "core/input.h"
#include "core/types.h"
#include "core/coroutines.h"
#include "engine/imgui.h"
#include "engine/imgui_serialization.h"
#include "fileio/fileio.h"
#include "serialization/json.h"
#include "game/pong.h"

struct GameData
{
    Imgui::Font ui_font = {};
    bool show_text = false;

    Pong::GameState state;
};

void init(Application& app)
{
    GameData& game = *(GameData*) app.data;

    {   // Load Font
        Bytes bytes  = file_load_bytes(ref("assets/fonts/bell.font.bytes"));
        game.ui_font = Imgui::font_load_from_bytes(bytes);
        free(bytes);
    }

    Pong::game_state_reset(game.state, app, true);
}

void update(Application& app)
{
    if (Input::get_key_down(Key::ESCAPE))
    {
        app.is_running = false;
        return;
    }

    GameData& game = *(GameData*) app.data;

    Pong::paddle_update(game.state, app);
    Pong::ball_update(game.state, app);
}

void render(Application& app)
{
    GameData& game = *(GameData*) app.data;

    Imgui::begin();

    {   // Title
        const String text = ref("Pong!");
        const Vector2 size = Imgui::get_rendered_text_size(text, game.ui_font);
        const Vector3 top_left = 0.5f * Vector3 { app.window.ref_width - size.x, 10.0f, 0.0f };
        Imgui::render_text(text, game.ui_font, top_left);
    }

    {   // Score
        char buffer[128];

        {   // Player 1
            String score_string = ref(buffer, 128);
            to_string(score_string, game.state.player_scores[0]);

            const Vector3 top_left = Vector3 { 0.0f, 0.0f, 0.0f };
            Imgui::render_text(score_string, game.ui_font, top_left);
        }

        {   // Player 2
            String score_string = ref(buffer, 128);
            to_string(score_string, game.state.player_scores[1]);

            const Vector2 size = Imgui::get_rendered_text_size(score_string, game.ui_font);
            const Vector3 top_left = Vector3 { app.window.ref_width - size.x, 0.0f, 0.0f };
            Imgui::render_text(score_string, game.ui_font, top_left);
        }
    }

    {   // Draw Paddles
        Imgui::Rect rect;
        rect.size = Vector2 { Pong::paddle_size_x, Pong::paddle_size_y };

        rect.top_left = Vector3 { 0.0f, game.state.paddle_positions[0], 0.0f };
        Imgui::render_rect(rect, Vector4 { 0.5f, 0.5f, 0.5f, 1.0f });

        rect.top_left = Vector3 { (f32) app.window.ref_width - Pong::paddle_size_x, game.state.paddle_positions[1], 0.0f };
        Imgui::render_rect(rect, Vector4 { 0.5f, 0.5f, 0.5f, 1.0f });

        rect.size = Vector2 { Pong::ball_dimension, Pong::ball_dimension };
        rect.top_left  = Vector3 { game.state.ball_position.x, game.state.ball_position.y, 0.0f };
        Imgui::render_rect(rect, Vector4 { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    Imgui::end();
}

void shutdown(Application& app)
{
    GameData& game = *(GameData*) app.data;

    free(game.ui_font);
    
    platform_free(app.data);
}

void on_window_resize(Application& app)
{
    GameData& game = *(GameData*) app.data;

    Pong::game_resized_window(game.state, app);
}

void create_app(Application& app)
{
    app.window.x = 500;
    app.window.y = 500;
    app.window.width  = 1024;
    app.window.height = 720;
    app.window.ref_height = 1080;
    app.window.name = ref("Pong!");

    app.on_init     = init;
    app.on_update   = update;
    app.on_render   = render;
    app.on_shutdown = shutdown;

    // Callbacks
    app.on_window_resize = on_window_resize;

    app.data = (void*) platform_allocate(sizeof(GameData));
    gn_assert_with_message(app.data, "Couldn't allocate game data!");
    (*(GameData*) app.data) = GameData();    // Initialize to default values
}