#pragma once

#include "core/types.h"
#include "containers/bytes.h"
#include "containers/darray.h"

#include "application/application.h"
#include "game.h"

namespace Package
{

Bytes pack_assets();
Bytes pack_settings(const Application& app, const GameData& data);
Bytes pack_settings_default(const GameData& data);
String pack_shaders();

}