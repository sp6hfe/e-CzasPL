#include <DataDecoder/DataDecoder.hpp>

#include <iostream>
#include <optional>
#include <stdio.h>
#include <time.h>

using namespace std;

static constexpr uint8_t RAW_DATA_SAMPLES_PER_BIT{10U};

union ByteTranslator {
  char bytes[2U];
  uint16_t uint16;
};

int main() {
  auto handleFrameReception{[](std::pair<const eczas::DataDecoder::TimeData&, uint32_t> frameDetails) {
    static constexpr uint32_t secondsInHour{3600U};

    static auto frameNo{0U};

    auto localTimeOffsetInHours{static_cast<uint8_t>(frameDetails.first.offset)};
    time_t utcTime{frameDetails.first.utcUnixTimestamp};
    time_t localUtcTime{frameDetails.first.utcUnixTimestamp + (localTimeOffsetInHours * secondsInHour)};

    printf("\nTime frame %d (at sample %d): ", ++frameNo, frameDetails.second);
    printf("\n> seconds since year 2000: %d", frameDetails.first.utcTimestamp);
    printf("\n> seconds since year 1970: %d", frameDetails.first.utcUnixTimestamp);
    printf("\n> local time zone offset in hours: +%d", localTimeOffsetInHours);
    printf("\n> UTC time : %s", asctime(gmtime(&utcTime)));
    printf("> local time (UTC+%d): %s", localTimeOffsetInHours, asctime(gmtime(&localUtcTime)));

    if (frameDetails.first.timeZoneChangeAnnouncement) {
      printf("> time zone offset change announced");
    } else {
      printf("> no time zone offset change announced");
    }

    if (frameDetails.first.leapSecondAnnounced) {
      if (frameDetails.first.leapSecondPositive) {
        printf("\n> positive leap second announced");
      } else {
        printf("\n> negative leap second announced");
      }
    } else {
      printf("\n> no leap second announced");
    }

    switch (frameDetails.first.transmitterState) {
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Day:
        printf("\n> planned transmitter maintenance for 1 day");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Week:
        printf("\n> planned transmitter maintenance for 1 week");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenanceOver1Week:
        printf("\n> planned transmitter maintenance for over 1 week");
        break;
      default:
        printf("\n> transmitter working OK");
        break;
    }
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