;; Unbounded recursion through call_ref. A safe interpreter must trap (stack overflow), never crash.
(module
  (type $t (func (param i32) (result i32)))
  (elem declare func $f)
  (func $f (type $t) (param i32) (result i32)
    (call_ref $t (i32.add (local.get 0) (i32.const 1)) (ref.func $f)))
  (func (export "rec") (param i32) (result i32)
    (call $f (local.get 0))))
