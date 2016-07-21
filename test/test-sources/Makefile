# Copyright 2016 WebAssembly Community Group participants
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Runs test on decompression. To update the tests based on spec, run:
#
# make -f Makefile.update

include Makefile.common

test: $(LOCAL_TESTS)

$(LOCAL_TESTS): $(LOCAL_SEXPS_DIR)/%.test: $(LOCAL_SEXPS_DIR)/%.wasm
	@# Read using no default algorithms
	$(EXEC_DIR)/decompress -m -i $< | diff - $<
	@# Read using default algorithms
	$(EXEC_DIR)/decompress -d defaults.wasm -m -i $< | diff - $<
