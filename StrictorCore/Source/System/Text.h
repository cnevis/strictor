#pragma once

#include <filesystem>
#include <string>

std::string utf8(std::filesystem::path const& p);
std::filesystem::path utf8Path(std::string const& s);
std::wstring wide(std::string const& s);
std::string trim(std::string const& s);

bool upperLetter(char c);
bool lowerLetter(char c);
bool digit(char c);
std::string lowercase(std::string const& s);
std::string uppercase(std::string const& s);
bool glob(std::string const& pattern, std::string const& s);
