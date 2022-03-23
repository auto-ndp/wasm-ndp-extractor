
#include "wndpe/wndpe.h"

#include <pass.h>
#include <shell-interface.h>
#include <support/command-line.h>
#include <support/debug.h>
#include <support/file.h>
#include <wasm-binary.h>
#include <wasm-interpreter.h>
#include <wasm-io.h>
#include <wasm-s-parser.h>
#include <wasm-validator.h>

#include <fmt/core.h>

namespace wasm {
Pass* createNdpOutliningPass();
}

namespace wndpe {

std::unique_ptr<wasm::Module> loadModule(const std::filesystem::path& path) {
  std::unique_ptr wmod = std::make_unique<wasm::Module>();
  wmod->features.setMVP();
  wmod->features.setAtomics();
  wmod->features.setBulkMemory();
  wmod->features.setMultivalue();
  wmod->features.setMutableGlobals();
  wmod->features.setReferenceTypes();
  wmod->features.setSIMD();
  wmod->features.setRelaxedSIMD();
  wasm::ModuleReader reader;
  reader.setProfile(wasm::IRProfile::Normal);
  try {
    reader.read(path.string(), *wmod, "");
  } catch (wasm::ParseException& p) {
    p.dump(std::cerr);
    std::cerr << '\n';
    fmt::print(stderr, "error parsing wasm\n");
    std::exit(1);
  } catch (wasm::MapParseException& p) {
    p.dump(std::cerr);
    std::cerr << '\n';
    fmt::print(stderr, "error parsing wasm source map\n");
    std::exit(1);
  } catch (std::bad_alloc&) {
    fmt::print(stderr,
               "error building module, std::bad_alloc (possibly invalid "
               "request for silly amounts of memory)\n");
    std::exit(1);
  }
  if (!wasm::WasmValidator().validate(*wmod)) {
    fmt::print(stderr, "Error validating wasm\n");
    std::exit(1);
  }
  return wmod;
}

void runOutliningPasses(wasm::Module& wmod) {
  wasm::PassRunner runner{&wmod};
  runner.add(std::unique_ptr<wasm::Pass>(wasm::createNdpOutliningPass()));
  runner.add("dce");
  runner.run();
}

void writeWat(wasm::Module& wmod, const std::filesystem::path& outPath) {
  wasm::ModuleWriter writer;
  Colors::setEnabled(false);
  writer.setBinary(false);
  writer.setDebugInfo(false);
  writer.writeText(wmod, outPath.string());
}

} // namespace wndpe
