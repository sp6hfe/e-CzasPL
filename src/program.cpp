#include <DataDecoder/DataDecoder.hpp>

#include <iostream>
#include <optional>
#include <stdio.h>

using namespace std;

static constexpr uint8_t RAW_DATA_SAMPLES_PER_BIT{10U};

union ByteTranslator {
  char bytes[2U];
  uint16_t uint16;
};

void printBinaryValue(uint8_t value) {
  for (auto bit{0U}; bit < 8U; bit++) {
    const auto bitValueOne{(value & 0x80) ? true : false};
    if (bitValueOne) {
      printf("1");
    } else {
      printf("0");
    }
    value <<= 1U;
  }
}

int main() {
  auto handleFrameReception{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> frameDetails) {
    static auto frameNo{0U};

    printf("\nFrame %d at %d: ", ++frameNo, frameDetails.second);
    for (auto byte : frameDetails.first) {
      printBinaryValue(byte);
      printf(" ");
    }
  }};

  ByteTranslator translator{};
  eczas::DataDecoder decoder{RAW_DATA_SAMPLES_PER_BIT};

  decoder.registerTimeFrameReceptionCallback(handleFrameReception);

  printf("\ne-CzasPL Radio C++ reference data decoder by SP6HFE\n");

  uint32_t sampleNo{0U};

  for (;;) {
    const auto& result{std::cin.read(&translator.bytes[0], 2U)};
    if (not result.good()) {
      break;
    }

    const auto bufferFull{decoder.processNewSample(translator.uint16)};
    if (bufferFull) {
      printf("\nE: Stream buffer full");
    }

    sampleNo++;
  }

  printf("\n\nProcessed %d samples.\n", sampleNo);

  return 0;
}