#pragma once

#include "Clang/Toolchain.h"

#include <filesystem>

Toolchain const& toolchain();
std::filesystem::path strictorExe();
