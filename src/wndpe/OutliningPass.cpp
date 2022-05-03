#include <cfg/cfg-traversal.h>
#include <ir/iteration.h>
#include <ir/properties.h>
#include <ir/type-updating.h>
#include <pass.h>
#include <vector>
#include <wasm-builder.h>
#include <wasm.h>

#include <mutex>

#include <fmt/core.h>

namespace wasm {

struct BlockInfo {
  Expression* node = nullptr;
  bool onMainPath = false;
  bool onOutlinedPath = false;
  bool hasBeginDirectly = false;
  bool hasEndDirectly = false;
};

static std::mutex OutliningModuleMutex;

struct NdpOutliningPass
  : public WalkerPass<
      CFGWalker<NdpOutliningPass, Visitor<NdpOutliningPass>, BlockInfo>> {

  using Parent = WalkerPass<
    CFGWalker<NdpOutliningPass, Visitor<NdpOutliningPass>, BlockInfo>>;

  bool needsOutlining = false;

  bool isFunctionParallel() override { return true; }

  Pass* create() override { return new NdpOutliningPass; }

  BlockInfo nextInfo;

  BasicBlock* makeBasicBlock() {
    auto* bb = new BasicBlock();
    auto** currp = getCurrentPointer();
    bb->contents = std::move(nextInfo);
    nextInfo = BlockInfo{};
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
    bool putInOwnBlock = false;
    if (curr->target == intrnOutlineBegin) {
      nextInfo.hasBeginDirectly = true;
      putInOwnBlock = true;
      needsOutlining = true;
    } else if (curr->target == intrnOutlineEnd) {
      nextInfo.hasEndDirectly = true;
      putInOwnBlock = true;
    } else if (curr->target == intrnOutlineCall) {
      //
    }
    if (putInOwnBlock) {
      Builder builder(*getModule());
      replaceCurrent(builder.makeBlock("outlining_block", curr));
      doEndBlock(this, getCurrentPointer());
    }
  }

  void doWalkFunction(Function* oldFunction) {
    // Build the CFG.
    Parent::doWalkFunction(oldFunction);
    if (basicBlocks.empty() || this->entry == nullptr) {
      return;
    }
    if (!needsOutlining) {
      return;
    }
    fmt::print(stderr, "Will outline {}\n", oldFunction->name.c_str());

    // Mark basic blocks as on/off/both paths
    BasicBlock *outliningEntryBlock{}, *outliningExitBlock{};
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
        if (info.node != nullptr) {
          std::string bname = getExpressionName(info.node);
          if (Block* b = info.node->dynCast<Block>(); b && !b->name.isNull()) {
            bname = b->name.c_str();
          }
          fmt::print(stderr,
                     "Visiting BB {} begin:{} end:{}\n",
                     bname,
                     info.hasBeginDirectly,
                     info.hasEndDirectly);
        }
        if (info.hasBeginDirectly && !e.onOutlinedPath) {
          if (outliningEntryBlock != nullptr) {
            throw std::runtime_error("Found multiple outlining entry markers");
          }
          outliningEntryBlock = e.bb;
        }
        if (info.hasEndDirectly && e.onOutlinedPath) {
          if (outliningExitBlock != nullptr) {
            throw std::runtime_error("Found multiple outlining exit markers");
          }
          outliningExitBlock = e.bb;
        }
      }
    }

    // give debug names
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
      std::string bname = getExpressionName(info.node);
      if (Block* b = info.node->dynCast<Block>(); b && !b->name.isNull()) {
        bname = b->name.c_str();
      }
      fmt::print(stderr,
                 "BB-info {} begin:{} end:{} onMain:{} onOutline:{}\n",
                 bname,
                 info.hasBeginDirectly,
                 info.hasEndDirectly,
                 info.onMainPath,
                 info.onOutlinedPath);
    }

    // duplicate function
    // arguments: comm block ptr, size
    // returns: i32: did it do an early return
    Builder builder(*getModule());
    bool is64bit = getModule()->features.hasMemory64();
    auto ptrType = is64bit ? wasm::Type::i64 : wasm::Type::i32;
    Name newFnName = fmt::format("{}$outlined", oldFunction->name.c_str());
    {
      std::vector<NameType> args;
      args.emplace_back("comm_block_ptr", ptrType);
      args.emplace_back("comm_block_size", ptrType);
      std::unique_ptr<Function> newFunction =
        builder.makeFunction(newFnName,
                             std::move(args),
                             Signature(Type{ptrType, ptrType}, Type::i32),
                             {},
                             builder.makeBlock());
      //
      newFunction->body = ExpressionManipulator::flexibleCopy(
        oldFunction->body, *getModule(), [&](Expression* e) -> Expression* {
          switch (e->_id) {
            case Expression::CallId: {
              Call* call = e->cast<Call>();
              if (call->target == intrnOutlineBegin) {
                return builder.makeBlock(intrnOutlineJumpInsideLabel,
                                         builder.makeNop());
              } else if (call->target == intrnOutlineEnd) {
                return builder.makeReturn(builder.makeConst<int32_t>(0));
              }
              break;
            }
            case Expression::ReturnId: {
              Return* ret = e->cast<Return>();
              return builder.makeBlock(
                {builder.makeDrop(ret->value),
                 builder.makeReturn(builder.makeConst<int32_t>(1))});
            }
            default:
              break;
          }
          return nullptr;
        });
      newFunction->body =
        builder.makeBlock({builder.makeBreak(intrnOutlineJumpInsideLabel),
                           builder.makeDrop(newFunction->body),
                           builder.makeConst<int32_t>(1)},
                          wasm::Type::i32);
      {
        std::lock_guard _l(OutliningModuleMutex);
        getModule()->addFunction(std::move(newFunction));
      }
    }
  }

private:
  Name intrnOutlineJumpInsideLabel = "__wndpe_outlined_start_target";
  Name intrnOutlineBegin = "__wndpe_outline_begin";
  Name intrnOutlineEnd = "__wndpe_outline_end";
  Name intrnOutlineCall = "__wndpe_outline_call";
  Name intrnOutlineAlloc = "__wndpe_outline_alloc";
  Name intrnOutlineFree = "__wndpe_outline_free";
};

Pass* createNdpOutliningPass() { return new NdpOutliningPass(); }

} // namespace wasm
