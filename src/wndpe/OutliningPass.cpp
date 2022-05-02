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
  Expression* node = nullptr;
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

  BasicBlock* makeBasicBlock() {
    auto* bb = new BasicBlock();
    auto** currp = getCurrentPointer();
    bb->contents.node = currp ? *currp : nullptr;
    return bb;
  }

  static void doEndBlock(NdpOutliningPass* self, Expression** currp) {
    auto* curr = (*currp)->cast<Block>();
    // always make a new block, even if it's trivial with 1 entry, 1 exit
    auto* last = self->currBasicBlock;
    self->startBasicBlock();
    self->link(last, self->currBasicBlock); // fallthrough
    auto iter = self->branches.find(curr);
    if (iter == self->branches.end()) {
      return;
    }
    auto& origins = iter->second;
    if (origins.size() == 0) {
      return;
    }
    // branches to the new one
    for (auto* origin : origins) {
      self->link(origin, self->currBasicBlock);
    }
    self->branches.erase(curr);
  }

  void visitFunction(Function* func) {
    fmt::print(stderr, "Visiting function {}\n", func->name.c_str());
  }

  void visitCall(Call* curr) {
    fmt::print(stderr, "Visiting call to {}\n", curr->target.c_str());
    if (curr->target == intrnOutlineBegin) {
      if (currBasicBlock) {
        currBasicBlock->contents.hasBeginDirectly = true;
      }
      Builder builder(*getModule());
      replaceCurrent(
        builder.makeCall(intrnOutlineCall, curr->operands, curr->type));
      needsOutlining = true;
    } else if (curr->target == intrnOutlineEnd) {
      if (currBasicBlock) {
        currBasicBlock->contents.hasEndDirectly = true;
      }
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
        if (info.node != nullptr) {
          fmt::print(stderr, "Visiting BB {}\n", getExpressionName(info.node));
        }
        if (info.hasBeginDirectly && info.hasEndDirectly) {
          throw std::runtime_error(
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

    for (auto& bb : basicBlocks) {
      if (!bb) {
        continue;
      }
      BlockInfo& info = bb->contents;
      if (!info.node) {
        continue;
      }
      Block* b = info.node->dynCast<Block>();
      if (!b) {
        continue;
      }
      if (info.onMainPath) {
        if (b->name.size() == 0) {
          b->name.set("block");
        }
        std::string new_name = fmt::format("{}_onmainpath", b->name.c_str());
        b->name.set(new_name.c_str(), false);
      }
      if (info.onOutlinedPath) {
        if (b->name.size() == 0) {
          b->name.set("block");
        }
        std::string new_name =
          fmt::format("{}_onoutlinedpath", b->name.c_str());
        b->name.set(new_name.c_str(), false);
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
