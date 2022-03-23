#include "wndpe/wndpe.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <fmt/core.h>

using std::cout, std::cerr, std::endl;

constexpr std::string_view TEST_INPUT_EXT = ".input.wat";
constexpr std::string_view TEST_OUTPUT_EXT = ".output.wat";
constexpr std::string_view TEST_DEBUG_EXT = ".dbg.wat";

void runOutliningTests() {
  namespace fs = std::filesystem;
  fs::path dirPath{"tests/outlining"};
  if (!fs::is_directory(dirPath)) {
    fmt::print(stderr, "{} test directory not found.\n", dirPath.string());
    std::exit(1);
  }
  for (const auto& dent : fs::directory_iterator(dirPath)) {
    std::string spath = dent.path().string();
    if (!dent.is_regular_file() || !spath.ends_with(TEST_INPUT_EXT)) {
      continue;
    }
    fmt::print(stdout, "Test {}\n", spath);
    std::string opath = spath;
    opath.replace(spath.size() - TEST_INPUT_EXT.size(),
                  TEST_INPUT_EXT.size(),
                  TEST_DEBUG_EXT);
    {
      auto wmod = wndpe::loadModule(dent.path());
      wndpe::runOutliningPasses(*wmod);
      wndpe::writeWat(*wmod, opath);
    }
  }
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  runOutliningTests();
  return 0;
}
