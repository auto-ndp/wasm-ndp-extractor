#ifndef WNDPE_H_INCLUDED
#define WNDPE_H_INCLUDED 1

#include <cstdint>
#include <cstdlib>
#include <filesystem>

#include <wasm.h>

namespace wndpe {

std::unique_ptr<wasm::Module> loadModule(const std::filesystem::path& path);

void runOutliningPasses(wasm::Module& wmod);

void writeWat(wasm::Module& wmod, const std::filesystem::path& outPath);

} // namespace wndpe

#endif
