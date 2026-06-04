#pragma once
#include "core.h"
#include <string>

namespace mg {

bool loadRih(const std::string& path, Rih& out);
bool loadRihW(const wchar_t* path, Rih& out);

} // namespace mg
