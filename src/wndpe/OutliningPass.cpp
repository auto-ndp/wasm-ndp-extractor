#include <ir/iteration.h>
#include <ir/properties.h>
#include <ir/type-updating.h>
#include <pass.h>
#include <vector>
#include <wasm-builder.h>
#include <wasm.h>

namespace wasm {

struct NdpOutliningPass
    : public WalkerPass<PostWalker<
          NdpOutliningPass, UnifiedExpressionVisitor<NdpOutliningPass>>> {

  bool isFunctionParallel() override { return true; }

  Pass *create() override { return new NdpOutliningPass; }

  void visitCall(Call *curr) {
    if (curr->target == intrnOutlineBegin) {
      Builder builder(*getModule());
      replaceCurrent(builder.makeCall(intrnOutlineCall, curr->operands, curr->type));
    } else if(curr->target == intrnOutlineEnd) {
      //
    } else if(curr->target == intrnOutlineCall) {
      //
    }
  }

private:
  Name intrnOutlineBegin = "__wndpe_outline_begin";
  Name intrnOutlineEnd = "__wndpe_outline_end";
  Name intrnOutlineCall = "__wndpe_outline_call";
};

Pass *createNdpOutliningPass() { return new NdpOutliningPass(); }

} // namespace wasm
