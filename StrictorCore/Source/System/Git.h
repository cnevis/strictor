#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

std::vector<std::filesystem::path> gitChangedFiles();
std::optional<std::string> gitHeadText(std::filesystem::path const& file);
