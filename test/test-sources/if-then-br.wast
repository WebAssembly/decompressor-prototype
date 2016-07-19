;; The br expression breaks out of its if, without a value. Normally, the type
;; of the true branch would be i32, because the last statement has the type
;; i32. Since the br has no value, though, the true branch also has no value
;; so (i32.const 2) must be discarded. Since the true branch has no value, the
;; false branch must not have a value either, so (i32.const 3) is discarded as
;; well.

(module
  (func $f (param i32) (result i32)
    (local i32)
    (if
      (get_local 0)
      (then $l
        (br $l)
        (i32.const 2))
      (else
        (i32.const 3)))
  (i32.const 2))

  (export "e1" $e1)
  (func $e1 (result i32)
    (call $f (i32.const 0)))

  (export "e2" $e2)
  (func $e2 (result i32)
    (call $f (i32.const 1))))
