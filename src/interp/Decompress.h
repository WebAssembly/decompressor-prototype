/* -*- C++ -*- */
/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* C API to the decompressor interpreter */

#ifndef DECOMPRESSOR_SRC_INTERP_DECOMPRESS_H
#define DECOMPRESSOR_SRC_INTERP_DECOMPRESS_H

#include <stdint.h>

extern "C" {

#define DECOMPRESSOR_SUCCESS (-1)
#define DECOMPRESSOR_ERROR (-2)

/* Returns an allocated and initialized decompressor. */
extern void* create_decompressor();

/* Creates a buffer to pass data in/out. D is the decompressor and Size is the
 * size of buffer to use.
 */
extern uint8_t* get_decompressor_buffer(void* D, int32_t Size);

/* Resume decopmression, assuming the buffer contains Size bytes to read.  If
 * non-negative, returns the number of output bytes available.  If negative,
 * either DECOMPRESSOR_SUCCESS or DECOMPRESSOR_ERROR. NOTE: If Size == 0, the
 * code assumes that no more input will be provided (i.e. in all subsequent
 * calls, Size == 0).
 */
extern int32_t resume_decompression(void* D, int32_t Size);

/* Fetch the next Size bytes and put into the decompression buffer.
 * Returns true if successful.
 */
extern bool fetch_decompressor_output(void* D, int32_t Size);

/* Clean up D and then deallocates. */
extern void destroy_decompressor(void* D);
}

#endif  // DECOMPRESSOR_SRC_INTERP_DECOMPRESS_H
