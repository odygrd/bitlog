#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace bitlog::testing
{
// Convert the given file to a vector
std::vector<std::string> file_contents(std::filesystem::path const& filename)
{
  std::ifstream out_file(filename.string());

  std::vector<std::string> lines;

  for (std::string current_line; getline(out_file, current_line);)
  {
    lines.push_back(current_line);
  }

  return lines;
}

// Convert the given file to a vector
std::vector<std::wstring> wfile_contents(std::filesystem::path const& filename)
{
  std::wifstream out_file(filename.string());

  std::vector<std::wstring> lines;

  for (std::wstring current_line; getline(out_file, current_line);)
  {
    lines.push_back(current_line);
  }

  return lines;
}

// Search a vector for the given string
bool file_contains(std::vector<std::string> const& file_vector, std::string const& search_string)
{
  auto const search =
    std::find_if(file_vector.cbegin(), file_vector.cend(), [&search_string](std::string const& elem)
                 { return elem.find(search_string) != std::string::npos; });

  bool const success = search != file_vector.cend();

  if (!success)
  {
    // We failed to find and we will log for diagnostic reasons
    std::cout << "Failed to find '" << search_string << "' in:\n";
    for (auto const& line : file_vector)
    {
      std::cout << "'" << line << "'\n";
    }
  }

  return success;
}
} // namespace bitlog::testing