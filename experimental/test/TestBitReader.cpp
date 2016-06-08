#include "BitStreamReader.h"
#include "FileReader.h"
#include "FileWriter.h"

#include <climits>
#include <cstring>
#include <memory>
#include <unistd.h>

using namespace wasm::decode;

int main(int argc, char* argv[]) {
  (void) argc;
  (void) argv;
  bool ReadNibbles = false;
  bool TestJump = false;
  for (int i = 1; i < argc; ++i){
    if (strcmp(argv[i], "-nibble") == 0) {
      ReadNibbles = true;
    } else if (strcmp(argv[i], "-jump") == 0) {
      TestJump = true;
    } else {
      fprintf(stderr, "Option not understood: %s", argv[i]);
      return 1;
    }
  }
  auto InputReader(FdReader::create(STDIN_FILENO));
  auto BufferedInput = CircBuffer<uint8_t>::createReader(std::move(InputReader));
  auto Reader = BitStreamReader::create(BufferedInput);
  int Count = 0;
  if (TestJump) {
    size_t JumpBit = 9 * CHAR_BIT + 5;
    fprintf(stderr, "Jump to bit = %zu\n", JumpBit);
    Reader->jumpToBit(9 * CHAR_BIT + 5);
    fprintf(stderr, "readFixed(3) = %u\n", Reader->readFixed32(3));
  }
  while (!Reader->atEof()) {
    ++Count;
    uint8_t Byte = 0;
    if (ReadNibbles) {
      uint8_t Nibble3 = Reader->readFixed32(3);
      uint8_t Nibble5 = Reader->readFixed32(5);
      fprintf(stderr, "       Nibble(3) = %hhu, Nibble(5) = %hhu\n",
              Nibble3, Nibble5);
      Byte = (Nibble3 << 5) | Nibble5;
    } else {
      Byte = Reader->readFixed32(8);
    }
    fprintf(stderr, "[%d] = %d = '%c'\n", Count, static_cast<int>(Byte),
            static_cast<char>(Byte));
  }
  return 0;
}
