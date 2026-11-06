;; Fox Red Labs test app for wasm3 on a microcontroller. No linear memory, so it runs where 64 kB of memory does not exist.
;; Imports (module "env"): putc(char), led(on), delay_ms(ms), print_i32(value, prints decimal and a newline).
;; Exports: run(blinks) says hello and blinks the LED; fib(n) recursive Fibonacci; bench(n) an integer mixing loop.
(module
  (import "env" "putc" (func $putc (param i32)))
  (import "env" "led" (func $led (param i32)))
  (import "env" "delay_ms" (func $delay (param i32)))
  (import "env" "print_i32" (func $print_i32 (param i32)))

  (func $run (export "run") (param $blinks i32) (result i32)
    (local $i i32)
    ;; "hello from wasm\n"
    (call $putc (i32.const 104)) (call $putc (i32.const 101)) (call $putc (i32.const 108)) (call $putc (i32.const 108))
    (call $putc (i32.const 111)) (call $putc (i32.const 32))  (call $putc (i32.const 102)) (call $putc (i32.const 114))
    (call $putc (i32.const 111)) (call $putc (i32.const 109)) (call $putc (i32.const 32))  (call $putc (i32.const 119))
    (call $putc (i32.const 97))  (call $putc (i32.const 115)) (call $putc (i32.const 109)) (call $putc (i32.const 10))
    (block $done
      (loop $again
        (br_if $done (i32.ge_u (local.get $i) (local.get $blinks)))
        (call $led (i32.const 1))
        (call $delay (i32.const 200))
        (call $led (i32.const 0))
        (call $delay (i32.const 200))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (call $print_i32 (local.get $i))
        (br $again)))
    (local.get $i))

  (func $fib (export "fib") (param $n i32) (result i32)
    (if (result i32) (i32.lt_s (local.get $n) (i32.const 2))
      (then (local.get $n))
      (else (i32.add
        (call $fib (i32.sub (local.get $n) (i32.const 1)))
        (call $fib (i32.sub (local.get $n) (i32.const 2)))))))

  (func $bench (export "bench") (param $n i32) (result i32)
    (local $i i32) (local $acc i32) (local $x i32)
    (local.set $acc (i32.const 0x1234))
    (local.set $x (i32.const 1))
    (block $done
      (loop $l
        (br_if $done (i32.ge_u (local.get $i) (local.get $n)))
        (local.set $x (i32.add (i32.mul (local.get $x) (i32.const 1664525)) (i32.const 1013904223)))
        (local.set $acc (i32.add
          (i32.rotl (i32.xor (local.get $acc) (local.get $x)) (i32.const 5))
          (i32.shr_u (local.get $x) (i32.const 3))))
        (local.set $acc (i32.add (local.get $acc) (i32.and (local.get $x) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $l)))
    (local.get $acc))
)
