#include "ByteStreamReader.h"
#include "FileReader.h"
#include "FileWriter.h"

#include <memory>
#include <unistd.h>

using namespace wasm::decode;

int main(int argc, char* argv[]) {
  (void) argc;
  (void) argv;
  auto InputReader(FdReader::create(STDIN_FILENO));
  auto BufferedInput = CircBuffer<uint8_t>::createReader(std::move(InputReader));
  auto Reader = ByteStreamReader::create(BufferedInput);
  int Count = 0;
#if 0
  Reader->jumpToByte(10);
#endif
  while (!Reader->atEof()) {
    ++Count;
    uint8_t Byte = Reader->readUint8();
    fprintf(stderr, "[%d] = %d = '%c'\n", Count, static_cast<int>(Byte),
            static_cast<char>(Byte));
  }
  return 0;
}
