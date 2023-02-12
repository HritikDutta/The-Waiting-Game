#pragma once

#include "application/application.h"
#include "containers/bytes.h"
#include "game.h"

void game_load_assets(const Bytes& bytes, GameData& data);
void game_load_settings(const Bytes& bytes, Application& app, GameData& data);