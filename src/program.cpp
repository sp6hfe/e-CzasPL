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
    printf("(%02X) ", byte);
  }
}

int main() {
#ifdef DEBUG
  auto handleRawTimeFrameData{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> frameDetails) {
    printf("\n┌ Raw time frame (at sample %d):           ", frameDetails.second);
    printFrameContent(frameDetails.first);
  }};

  auto handleReedSolomonProcessedTimeFrameData{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> codeWordDetails) {
    printf("\n├ RS processed time frame (at sample %d):  ", codeWordDetails.second);
    printFrameContent(codeWordDetails.first);
  }};

  auto handleCrcProcessedTimeFrameData{[](std::pair<const eczas::DataDecoder::TimeFrame&, uint32_t> frameDetails) {
    printf("\n└ CRC processed time frame (at sample %d): ", frameDetails.second);
    printFrameContent(frameDetails.first);
  }};
#endif

  auto handleTimeData{[](std::pair<const eczas::DataDecoder::TimeData&, uint32_t> timeDetails) {
    static constexpr uint32_t secondsInHour{3600U};

    auto localTimeOffsetInHours{static_cast<uint8_t>(timeDetails.first.offset)};
    time_t utcTime{timeDetails.first.utcUnixTimestamp};
    time_t localUtcTime{timeDetails.first.utcUnixTimestamp + (localTimeOffsetInHours * secondsInHour)};

#ifdef DEBUG
    printf("\n┌ Time message (from time frame at sample %d)\n", timeDetails.second);
#else
    printf("\n┌ Time message\n");
#endif

    printf("├ UTC time          : %s", asctime(gmtime(&utcTime)));
    printf("├ local time (UTC+%d): %s", localTimeOffsetInHours, asctime(gmtime(&localUtcTime)));
    printf("├ seconds since year 2000: %d\n", timeDetails.first.utcTimestamp);
    printf("├ seconds since year 1970: %d\n", timeDetails.first.utcUnixTimestamp);

    if (timeDetails.first.timeZoneChangeAnnouncement) {
      printf("├ time zone offset change announced\n");
    } else {
      printf("├ no time zone offset change announced\n");
    }

    if (timeDetails.first.leapSecondAnnounced) {
      if (timeDetails.first.leapSecondPositive) {
        printf("├ positive leap second announced\n");
      } else {
        printf("├ negative leap second announced\n");
      }
    } else {
      printf("├ no leap second announced\n");
    }

    switch (timeDetails.first.transmitterState) {
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Day:
        printf("└ planned transmitter maintenance for 1 day\n");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenance1Week:
        printf("└ planned transmitter maintenance for 1 week\n");
        break;
      case eczas::DataDecoder::TransmitterState::PlannedMaintenanceOver1Week:
        printf("└ planned transmitter maintenance for over 1 week\n");
        break;
      default:
        printf("└ transmitter working OK\n");
        break;
    }
  }};

  ByteTranslator translator{};
  eczas::DataDecoder decoder{RAW_DATA_SAMPLES_PER_BIT};

#ifdef DEBUG
  decoder.registerRawTimeFrameCallback(handleRawTimeFrameData);
  decoder.registerRsProcessedTimeFrameCallback(handleReedSolomonProcessedTimeFrameData);
  decoder.registerCrcProcessedTimeFrameCallback(handleCrcProcessedTimeFrameData);
#endif

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

  printf("\nProcessed %d samples.\n", --sampleNo);

  return 0;
}