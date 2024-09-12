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

int main() {
  auto handleFrameReception{[](std::pair<const eczas::DataDecoder::TimeData&, uint32_t> frameDetails) {
    static auto frameNo{0U};

    printf("\nTime frame %d at %d: utcTimestamp: %d, utcUnixTimestamp: %d, localTimeOffset: %d",
      ++frameNo, frameDetails.second, frameDetails.first.utcTimestamp, frameDetails.first.utcUnixTimestamp, static_cast<uint8_t>(frameDetails.first.offset));
  }};

  ByteTranslator translator{};
  eczas::DataDecoder decoder{RAW_DATA_SAMPLES_PER_BIT};

  decoder.registerTimeDataReceptionCallback(handleFrameReception);

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