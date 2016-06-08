#include "DecodeDefs.h"

#include <stdio.h>
#include <cstdlib>

namespace wasm {

namespace decode {

void fatal(const char *Message) {
  fprintf(stderr, "%s\n", Message);
  exit(EXIT_FAILURE);
}

} // end of namespace decode

} // end of namespace wasm
