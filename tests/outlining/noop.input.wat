(module
  (func $noop (result i32)
    (block $b0)
    (block $b1
      (call $__wndpe_outline_begin)
    )
    (block $b2)
    (block $b3
      (call $__wndpe_outline_end)
    )
    (block $b4)
    (i32.const 0)
  )
  (import "__builtins" "__wndpe_outline_begin" (func $__wndpe_outline_begin))
  (import "__builtins" "__wndpe_outline_end" (func $__wndpe_outline_end))
  (export "noop" (func $noop))
)
