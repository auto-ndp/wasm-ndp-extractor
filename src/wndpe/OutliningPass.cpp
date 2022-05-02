#include <cfg/cfg-traversal.h>
#include <ir/iteration.h>
#include <ir/properties.h>
#include <ir/type-updating.h>
#include <pass.h>
#include <vector>
#include <wasm-builder.h>
#include <wasm.h>

#include <fmt/core.h>

namespace wasm {

struct BlockInfo {
  bool onMainPath = false;
  bool onOutlinedPath = false;
  bool hasBeginDirectly = false;
  bool hasEndDirectly = false;
};

struct NdpOutliningPass
  : public WalkerPass<
      CFGWalker<NdpOutliningPass, Visitor<NdpOutliningPass>, BlockInfo>> {

  using Parent = WalkerPass<
    CFGWalker<NdpOutliningPass, Visitor<NdpOutliningPass>, BlockInfo>>;

  bool needsOutlining = false;

  bool isFunctionParallel() override { return true; }

  Pass* create() override { return new NdpOutliningPass; }

  void visitFunction(Function* func) {
    fmt::print(stderr, "Visiting function {}\n", func->name.c_str());
  }

  void visitCall(Call* curr) {
    fmt::print(stderr, "Visiting call to {}\n", curr->target.c_str());
    if (curr->target == intrnOutlineBegin) {
      Builder builder(*getModule());
      replaceCurrent(
        builder.makeCall(intrnOutlineCall, curr->operands, curr->type));
      needsOutlining = true;
    } else if (curr->target == intrnOutlineEnd) {
      //
    } else if (curr->target == intrnOutlineCall) {
      //
    }
  }

  void doWalkFunction(Function* func) {
    // Build the CFG.
    Parent::doWalkFunction(func);
    if (basicBlocks.empty() || this->entry == nullptr) {
      return;
    }
    if (!needsOutlining) {
      return;
    }
    fmt::print(stderr, "Will outline {}\n", func->name.c_str());

    // Mark basic blocks as on/off/both paths
    {
      struct BBWalkEntry {
        BasicBlock* bb;
        bool onOutlinedPath;
      };
      std::vector<BBWalkEntry> remaining;
      remaining.reserve(16);
      remaining.emplace_back(entry, false);
      while (!remaining.empty()) {
        BBWalkEntry e = remaining.back();
        remaining.pop_back();
        BlockInfo& info = e.bb->contents;
        if (info.hasBeginDirectly && info.hasEndDirectly) {
          throw new std::runtime_error(
            "Same basic block has both begin and end outlining markers, check "
            "pass run order");
        }
        if (e.onOutlinedPath) {
          if (info.onOutlinedPath) {
            continue;
          }
          info.onOutlinedPath = true;
          for (BasicBlock* out : e.bb->out) {
            remaining.emplace_back(out, !info.hasEndDirectly);
          }
        } else {
          if (info.onMainPath) {
            continue;
          }
          info.onMainPath = true;
          for (BasicBlock* out : e.bb->out) {
            remaining.emplace_back(
              out, info.hasBeginDirectly && !info.hasEndDirectly);
          }
        }
      }
    }
  }

private:
  Name intrnOutlineBegin = "__wndpe_outline_begin";
  Name intrnOutlineEnd = "__wndpe_outline_end";
  Name intrnOutlineCall = "__wndpe_outline_call";
};

Pass* createNdpOutliningPass() { return new NdpOutliningPass(); }

} // namespace wasm
