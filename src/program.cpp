#include <DataDecoder/DataDecoder.hpp>
#include <Tools/Helpers.hpp>

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

void printFrameContent(const eczas::DataDecoder::TimeFrame& frame) {
  for (auto byte : frame) {
    tools::Helpers::printBinaryValue(byte);
    printf(" ");
  }
}

int main() {
  auto handleRawTimeFrameData{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> frameDetails) {
    printf("\n> Raw time frame (at sample %d):       ", frameDetails.second);
    printFrameContent(frameDetails.first);
  }};

  auto handleProcessedTimeFrameData{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> frameDetails) {
    printf("\n> Processed time frame (at sample %d): ", frameDetails.second);
    printFrameContent(frameDetails.first);
  }};

  auto handleTimeData{[](std::pair<const eczas::DataDecoder::TimeData&, uint32_t> timeDetails) {
    static constexpr uint32_t secondsInHour{3600U};

    auto localTimeOffsetInHours{static_cast<uint8_t>(timeDetails.first.offset)};
    time_t utcTime{timeDetails.first.utcUnixTimestamp};
    time_t localUtcTime{timeDetails.first.utcUnixTimestamp + (localTimeOffsetInHours * secondsInHour)};

    printf("\n> Time message (from time frame at sample %d): ", timeDetails.second);
    printf("\n- seconds since year 2000: %d", timeDetails.first.utcTimestamp);
    printf("\n- seconds since year 1970: %d", timeDetails.first.utcUnixTimestamp);
    printf("\n- local time zone offset in hours: +%d", localTimeOffsetInHours);
    printf("\n- UTC time : %s", asctime(gmtime(&utcTime)));
    printf("- local time (UTC+%d): %s", localTimeOffsetInHours, asctime(gmtime(&localUtcTime)));

    if (timeDetails.first.timeZoneChangeAnnouncement) {
      printf("- time zone offset change announced");
    } else {
      printf("- no time zone offset change announced");
    }

    if (timeDetails.first.leapSecondAnnounced) {
      if (timeDetails.first.leapSecondPositive) {
        printf("\n- positive leap second announced");
      } else {
        printf("\n- negative leap second announced");
      }
    } else {
      printf("\n- no leap second announced");
    }

    switch (timeDetails.first.transmitterState) {
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Day:
        printf("\n- planned transmitter maintenance for 1 day");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Week:
        printf("\n- planned transmitter maintenance for 1 week");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenanceOver1Week:
        printf("\n- planned transmitter maintenance for over 1 week");
        break;
      default:
        printf("\n- transmitter working OK");
        break;
    }
  }};

  ByteTranslator translator{};
  eczas::DataDecoder decoder{RAW_DATA_SAMPLES_PER_BIT};

  decoder.registerTimeFrameRawCallback(handleRawTimeFrameData);
  decoder.registerTimeFrameProcessedCallback(handleProcessedTimeFrameData);
  decoder.registerTimeDataCallback(handleTimeData);

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