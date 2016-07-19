# Copies test wast file (WASM s-expressions) to this directory, and then converts
# them to binary form. This allows us to easily build test cases.

EXEC_DIR = ../../build/bin
SEXP_WASM_DIR = ../../../sexpr-wasm-prototype

SPEC_SEXPS_DIR = ../../../spec/ml-proto/test
LOCAL_SEXPS_DIR = spec

# Note: the following spec test are not included because they do not generate a
# valid wasm file.
#	br_table.wast \
#	func-local-after-body.fail.wast \
#	func-local-before-param.fail.wast \
#	func-local-before-result.fail.wast \
#	func-param-after-body.fail.wast \
#	func-result-after-body.fail.wast \
#	func-result-before-param.fail.wast \
#	if_label_scope.fail.wast \
#	of_string-overflow-hex-u32.fail.wast \
#	of_string-overflow-hex-u64.fail.wast \
#	of_string-overflow-s32.fail.wast \
#	of_string-overflow-s64.fail.wast \
#	of_string-overflow-u32.fail.wast \
#	of_string-overflow-u64.fail.wast \

SEXPS = address.wast \
	binary.wast \
	block.wast \
	break-drop.wast \
	br_if.wast \
	br.wast \
	call_indirect.wast \
	call.wast \
	comments.wast \
	conversions.wast \
	endianness.wast \
	exports.wast \
	f32_cmp.wast \
	f32.wast \
	f64_cmp.wast \
	f64.wast \
	fac.wast \
	float_exprs.wast \
	float_literals.wast \
	float_memory.wast \
	float_misc.wast \
	forward.wast \
	func_ptrs.wast \
	func.wast \
	get_local.wast \
	i32.wast \
	i64.wast \
	imports.wast \
	int_exprs.wast \
	int_literals.wast \
	labels.wast \
	left-to-right.wast \
	loop.wast \
	memory_redundancy.wast \
	memory_trap.wast \
	memory.wast \
	names.wast \
	nan-propagation.wast \
	nop.wast \
	resizing.wast \
	return.wast \
	select.wast \
	set_local.wast \
	start.wast \
	store_retval.wast \
	switch.wast \
	traps.wast \
	typecheck.wast \
	unreachable.wast

SPEC_SEXPS_SRCS=$(patsubst %.wast, $(SPEC_SEXPS_DIR)/%.wast, $(SEXPS))
LOCAL_SEXPS_SRCS=$(patsubst %.wast, $(LOCAL_SEXPS_DIR)/%.wast, $(SEXPS))
LOCAL_SEXPS_BINS=$(patsubst %.wast, $(LOCAL_SEXPS_DIR)/%.wasm, $(SEXPS))
LOCAL_TESTS=$(patsubst %.wast, $(LOCAL_SEXPS_DIR)/%.test, $(SEXPS))

$(info -----------------------------------------------)
$(info ***NOTE*** Expect error messages while generating wasm files.)
$(info SEXPS = $(SEXPS))
$(info -----------------------------------------------)

test: $(LOCAL_TESTS)

# NOTE: update will have LOTS of error messages, but they can be ignored.
update: clean_binaries $(LOCAL_SEXPS_SRCS) $(LOCAL_SEXPS_BINS)

.PHONY: update

clean: clean_binaries
	rm -rf $(LOCAL_SEXPS_DIR)

.PHONY: clean

clean_binaries:
	rm -rf $(LOCAL_SEXPS_BINS)

$(LOCAL_SEXPS_SRCS): | $(LOCAL_SEXPS_DIR)

$(LOCAL_SEXPS_BINS): | $(LOCAL_SEXPS_DIR)

$(LOCAL_TESTS): | $(LOCAL_SEXPS_DIR)

$(LOCAL_SEXPS_DIR):
	mkdir -p $@

$(LOCAL_SEXPS_SRCS): $(LOCAL_SEXPS_DIR)/%.wast: $(SPEC_SEXPS_DIR)/%.wast
	cp $< $@

# NOTE: sexpr-wasm will have lots of error messages, but they do not block
# building the corresponding wasm files.
$(LOCAL_SEXPS_BINS): $(LOCAL_SEXPS_DIR)/%.wasm: $(LOCAL_SEXPS_DIR)/%.wast
	$(SEXP_WASM_DIR)/out/sexpr-wasm $< -o $@

$(LOCAL_TESTS): $(LOCAL_SEXPS_DIR)/%.test: $(LOCAL_SEXPS_DIR)/%.wasm
	$(EXEC_DIR)/decompress -m -i $< | diff - $<
