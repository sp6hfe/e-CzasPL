#include <DataDecoder/DataDecoder.hpp>
#include <Tools/Crc8.hpp>

#include <cstdlib>
#include <stdint.h>
#include <optional>
#include <tuple>
#include <utility>

#ifdef DEBUG
#include <stdio.h>
#endif

namespace eczas {

DataDecoder::DataDecoder(uint8_t streamSamplesPerBit) : _streamSamplesPerBit(streamSamplesPerBit) {
  _stream.fill(0);
  _correlator.fill(false);
  _sampleNo.fill(0U);
}

bool DataDecoder::processNewSample(int16_t sample) {
  // TODO: redo using circular buffer

  static auto sampleNo{0U};

  // move meaningful data left
  if (_meaningfulDataStartIndex < STREAM_SIZE) {
    if (_meaningfulDataStartIndex == LAST_STREAM_INDEX) {
      _stream[LAST_STREAM_INDEX - 1U] = _stream[LAST_STREAM_INDEX];
      _correlator[LAST_STREAM_INDEX - 1U] = _correlator[LAST_STREAM_INDEX];
      _sampleNo[LAST_STREAM_INDEX - 1U] = _sampleNo[LAST_STREAM_INDEX];
    } else {
      if (_meaningfulDataStartIndex != 0U) {
        _stream[_meaningfulDataStartIndex - 1U] = _stream[_meaningfulDataStartIndex];
        _correlator[_meaningfulDataStartIndex - 1U] = _correlator[_meaningfulDataStartIndex];
        _sampleNo[_meaningfulDataStartIndex - 1U] = _sampleNo[_meaningfulDataStartIndex];
      }

      for (uint16_t streamIndex{_meaningfulDataStartIndex}; streamIndex < LAST_STREAM_INDEX; streamIndex++) {
        _stream[streamIndex] = _stream[streamIndex + 1U];
        _correlator[streamIndex] = _correlator[streamIndex + 1U];
        _sampleNo[streamIndex] = _sampleNo[streamIndex + 1U];
      }
    }
  }

  // add new data
  _stream[LAST_STREAM_INDEX] = sample;
  _correlator[LAST_STREAM_INDEX] = false;
  _sampleNo[LAST_STREAM_INDEX] = sampleNo;

  // update fresh data index
  if (_meaningfulDataStartIndex) {
    _meaningfulDataStartIndex--;
  }

  // process incoming data
  calculateSyncWordCorrelation();

  // validate if frame start index is detectable
  const auto frameStartIndex{lookupFrameStartIndex()};

  if (frameStartIndex.has_value()) {
    // try to extract the data from the stream
    auto timeFrameGetter{getTimeFrameDataFromStream(frameStartIndex.value())};

    if (timeFrameGetter.has_value()) {
      auto [timeFrame, lastIndexOfTheTimeFrame] = timeFrameGetter.value();

      // validate synchronization word
      const auto frameSyncWordOk{(timeFrame.at(0) == static_cast<uint8_t>(SYNC_WORD >> 8U)) and (timeFrame.at(1) == static_cast<uint8_t>(SYNC_WORD & 0x00FF))};

      // validate time frame start byte
      const auto timeFrameStartByteOk{timeFrame.at(2) == TIME_FRAME_START_BYTE};

      // validate time frame static bits (3 MSb of byte 3 is 0b101)
      const auto timeFrameStaticBitsOk{(static_cast<uint8_t>(timeFrame.at(3) >> 5U) == 0x05)};

      // process time message data
      if (frameSyncWordOk and timeFrameStartByteOk and timeFrameStaticBitsOk) {
        TimeData timeData{
          .utcTimestamp = 0U,
          .utcUnixTimestamp = 0U,
          .offset = TimeZoneOffset::OffsetPlus0h,
          .timeZoneChangeAnnouncement = false,
          .leapSecondAnnounced = false,
          .leapSecondPositive = false,
          .transmitterState = TransmitterState::NormalOperation};

        // send raw time frame - if callback registered
        if (_timeFrameRawCallback) {
          _timeFrameRawCallback({timeFrame, _sampleNo[frameStartIndex.value()]});
        }

        // check CRC8 to see if any error correction is needed
        uint8_t crc8 = 0U;
        {
          tools::Crc8 crc{CRC8_POLYNOMIAL, CRC8_INIT_VALUE};
          crc.init();
          for (auto byteNo{3U}; byteNo < 8U; byteNo++) {
            crc.update(timeFrame.at(byteNo));
          }
          crc8 = crc.getCrc8();
        }
        const auto timeMessageDataValid{crc8 == timeFrame.at(11)};

        // lookup and correct errors (Reed-Solomon) - to be discussed with GUM
        if (not timeMessageDataValid) {
#ifdef DEBUG
          printf("\nX CRC8 issue - can't fix errors yet using Reed-Solomon data");
#endif
        }

        // proceed if data validation/correction was successful
        if (timeMessageDataValid) {
          // descramble time message (37 bytes starting at byte 3 bit 4 until byte 7 bit 0; 3 MSb of scrambling word are 0 (0x0A) so they won't affect message's static part)
          auto timeFrameByteNo{3U};
          for (auto scramblingByte : _scramblingWord) {
            timeFrame.at(timeFrameByteNo) ^= scramblingByte;

            // at the same time grab timestamp data
            switch (timeFrameByteNo) {
              case 3U:  // bit 3-7
                timeData.utcTimestamp += (timeFrame.at(timeFrameByteNo) & 0x1F);
                break;
              case 7U:  // MSb only
                timeData.utcTimestamp <<= 1U;
                timeData.utcTimestamp += ((timeFrame.at(timeFrameByteNo) & 0x80) ? 1U : 0U);
                break;
              default:  // full byte
                timeData.utcTimestamp <<= 8U;
                timeData.utcTimestamp += timeFrame.at(timeFrameByteNo);
                break;
            }

            timeFrameByteNo++;
          }

          // correct received timestamp as it means the number of 3[s] periods since beginning of the year 2000
          static constexpr uint32_t secondsBetweenYear1970And2000{946684800U};

          timeData.utcTimestamp *= 3U;
          timeData.utcUnixTimestamp = timeData.utcTimestamp + secondsBetweenYear1970And2000;

          // get the local time offset (bits TZ0 (6) and TZ1 (5)) - this should be sent other way around for simpler decoding
          const auto timeOffset{static_cast<uint8_t>((timeFrame.at(7U) >> 5) & 0x03)};
          switch (timeOffset) {
            case 0x01:
              timeData.offset = TimeZoneOffset::OffsetPlus2h;
              break;
            case 0x02:
              timeData.offset = TimeZoneOffset::OffsetPlus1h;
              break;
            case 0x03:
              timeData.offset = TimeZoneOffset::OffsetPlus3h;
              break;
            default:
              timeData.offset = TimeZoneOffset::OffsetPlus0h;
              break;
          }

          // get time zone change announcement (bit TZC(2))
          timeData.timeZoneChangeAnnouncement = ((static_cast<uint8_t>(timeFrame.at(7U) >> 2U) & 0x01) ? true : false);

          // extract leap second related information (bits LS(4) and LSS(3))
          timeData.leapSecondAnnounced = ((static_cast<uint8_t>(timeFrame.at(7U) >> 4U) & 0x01) ? true : false);
          timeData.leapSecondPositive = ((static_cast<uint8_t>(timeFrame.at(7U) >> 3U) & 0x01) ? true : false);

          // extract transmitter state (bits SK0 (1) and SK1 (0)) - this should be sent other way around for simpler decoding
          const auto transmitterState{static_cast<uint8_t>(timeFrame.at(7U) & 0x03)};
          switch (transmitterState) {
            case 0x01:
              timeData.transmitterState = TransmitterState::PlannedMaintenance1Week;
              break;
            case 0x02:
              timeData.transmitterState = TransmitterState::PlannedMaintenance1Day;
              break;
            case 0x03:
              timeData.transmitterState = TransmitterState::PlannedMaintenanceOver1Week;
              break;
            default:
              timeData.transmitterState = TransmitterState::NormalOperation;
              break;
          }

          // send processed time frame - if callback registered
          if (_timeFrameProcessedCallback) {
            _timeFrameProcessedCallback({timeFrame, _sampleNo[frameStartIndex.value()]});
          }

          // send time data - if callback received
          if (_timeDataCallback) {
            _timeDataCallback({timeData, _sampleNo[frameStartIndex.value()]});
          }
        }
      }

      // move stream meaningful data index beyond already extracted time frame (to prevent repeated detection)
      _meaningfulDataStartIndex = lastIndexOfTheTimeFrame;
    }
  }

  // update sample no for next iteration
  sampleNo++;

  // return if buffer is full
  return (_meaningfulDataStartIndex == 0U);
}

void DataDecoder::registerTimeDataCallback(TimeDataCallback callback) {
  _timeDataCallback = std::move(callback);
}

void DataDecoder::registerTimeFrameRawCallback(TimeFrameCallback callback) {
  _timeFrameRawCallback = std::move(callback);
}

void DataDecoder::registerTimeFrameProcessedCallback(TimeFrameCallback callback) {
  _timeFrameProcessedCallback = std::move(callback);
}

void DataDecoder::calculateSyncWordCorrelation() {
  /* Calculate correlation against 16 bit sync word 0x5555
     - LSb of the sync word is the last sample in the stream buffer
     - sync word bit samples used in calculation are spaced in buffer with _streamSamplesPerBit
     - MSb of the sync word is located (15 * _streamSamplesPerBit) bits back with respect to LSb sample
     - correlation is placed at sync word's MSb index to ease further localization of the frame start

     Data frames are separated with some fill-up time so they can start at full second.
     Before beginning of the sync word stream values are around value 0 (no carrier phase change).
     MSb of sync word is not detectable from the stream values due to no phase change observed (but 0-value is usable later on).
     Drop in stream's sample value below 0 (and lower hysteresis region) is an indication of the start of bit value 0 transmission.
     Jump in stream's sample value above 0 (and higher hysteresis region) is an indication of the start of bit value 1 transmission.
     In order to detect where sync word 0x5555 lay in the stream a correlation estimate is calculated on each new signal sample recption. */

  bool correlationDetected{true};

  // traverse stream to correlate according to 16 points spaced equally (every _streamSamplesPerBit)
  auto streamIndexToSyncWordBit{LAST_STREAM_INDEX};

  /* due to sync word value (alternating bit values) carrier phase changes are expected to be cyclic;
     sync word's LSb is located at stream end, more significant bits are accessed by jumping back by _streamSamplesPerBit */
  for (uint8_t syncWordBitNo{0U}; syncWordBitNo < SYNC_WORD_BITS_NO; syncWordBitNo++) {
    // for the sync word area stream samples should have meaningful magnitude
    if (not isSampleValueOutOfNoiseRegion(streamIndexToSyncWordBit)) {
      correlationDetected = false;
      break;
    }

    const auto syncWordBitValue{((SYNC_WORD >> syncWordBitNo) & static_cast<uint16_t>(0x0001)) != 0U};
    const auto streamBitValue{_stream[streamIndexToSyncWordBit] > 0};

    // they should exactly follow sync word's bit pattern
    if (syncWordBitValue != streamBitValue) {
      correlationDetected = false;
      break;
    }

    // to reach next sample a jump back is needed
    streamIndexToSyncWordBit -= _streamSamplesPerBit;
  }

  // store correlation value into the buffer at sync word's start index
  const auto syncWordStartIndex{static_cast<uint16_t>(LAST_STREAM_INDEX - static_cast<uint16_t>(static_cast<uint16_t>(SYNC_WORD_BITS_NO - 1U) * _streamSamplesPerBit))};
  _correlator[syncWordStartIndex] = correlationDetected;

  return;
}

bool DataDecoder::isSampleValueOutOfNoiseRegion(uint16_t index) {
  if (index >= STREAM_SIZE) {
#ifdef DEBUG
    printf("\nE: Sample index is out of range");
#endif
    return false;
  }

  const auto sampleValue{_stream[index]};
  return (abs(sampleValue) > STREAM_NOISE_HYSTERESIS);
}

std::optional<uint16_t> DataDecoder::detectSyncWordStartIndexByCorrelation() {
  /* Finding best match for sync word detection in data stream is by analysis of the correlation values. Correlation value periodically shows large positive numbers
   organized in sets with inverted parabola values. Those sets are with higher max values when approaching best match location.
   Due to the nature of the sync word (alternating bits) centers of local max correlation values are periodic with 2 * _streamSamplesPerBit spacing.
   Because of that a number of consecutive valid correlation regions maximas should be evaluated to detect highest one to tell the index
   of the frame start. Correlation values (calculated earlier) are stored in a way that they point MSb of the sync word. */

  const auto samplesNoForSyncWord{static_cast<uint16_t>(static_cast<uint16_t>(static_cast<uint16_t>(SYNC_WORD_BITS_NO - 1U) * _streamSamplesPerBit) + 1U)};
  const auto samplesNoWithoutCorrelationData{static_cast<uint16_t>(samplesNoForSyncWord - 1U)};      // correlation is calculated for sync word length backwards from newly added sample so for any newly added sample respective correlation is saved 15 bits (spaced every _streamSamplesPerBit) earlier at MSb index
  const auto samplesNoOfCorrelationMaxRepetition{static_cast<uint16_t>(_streamSamplesPerBit * 2U)};  // sync word correlation peaks few times around best match; peaks repetition relates to _streamSamplesPerBit

  bool syncWordDetected{false};
  uint16_t syncWordStartIndex{0U};

  const auto startIndexOfNotCalculatedCorrelatorData{static_cast<uint16_t>(STREAM_SIZE - samplesNoWithoutCorrelationData)};
  const auto minSamplesNoForSyncWordDetection{static_cast<uint16_t>(samplesNoForSyncWord + samplesNoOfCorrelationMaxRepetition)};

  // validate if it is worth doing any data analysis; at least sync word + some extra (having corelation value calculated) should fit in the buffer
  if (_meaningfulDataStartIndex > (startIndexOfNotCalculatedCorrelatorData - minSamplesNoForSyncWordDetection)) {
    return {};
  }

  // using correlation array find 1st best match for the sync word presence
  for (auto correlatorIndex{_meaningfulDataStartIndex}; correlatorIndex < startIndexOfNotCalculatedCorrelatorData; correlatorIndex++) {
    const auto correlationDetected{_correlator[correlatorIndex]};

    if (correlationDetected) {
      syncWordDetected = true;
      syncWordStartIndex = correlatorIndex;
      break;
    }
  }

  // if sync word is not detected invalidate relevant data
  if (not syncWordDetected) {
    _meaningfulDataStartIndex = startIndexOfNotCalculatedCorrelatorData;
    return {};
  }

  // all data before syncWordStartIndex is not usable
  _meaningfulDataStartIndex = syncWordStartIndex;
  return syncWordStartIndex;
}

std::optional<uint16_t> DataDecoder::lookupFrameStartIndex() {
  const auto syncWordCorrelationDetector{detectSyncWordStartIndexByCorrelation()};

  if (not syncWordCorrelationDetector.has_value()) {
    return {};
  }

  // when potential sync word start index was found (by correlation data) confirm it by value
  const auto syncWordStartIndex{syncWordCorrelationDetector.value()};
  const auto syncWordLocationValidated{validateSyncWordLocationInStream(syncWordStartIndex)};
  if (not syncWordLocationValidated) {
    // on wrong detection update _meaningfulDataStartIndex by moving it forward 1 RF bit (to allow for possibly good sync word detection with newer data)
    const auto indexOneRfBitBeyondWronglyDetectedSyncWordStart{static_cast<uint16_t>(syncWordStartIndex + _streamSamplesPerBit)};

    if (indexOneRfBitBeyondWronglyDetectedSyncWordStart > LAST_STREAM_INDEX) {
      _meaningfulDataStartIndex = STREAM_SIZE;
    } else {
      _meaningfulDataStartIndex = indexOneRfBitBeyondWronglyDetectedSyncWordStart;
    }

    return {};
  }

  return syncWordStartIndex;
}

std::optional<std::tuple<uint8_t, uint16_t, bool>> DataDecoder::getByteFromStream(uint16_t startIndex, bool initialBitValueIsOne) {
  // MSb is at startIndex, rest is spaced with _streamSamplesPerBit
  const auto lastIndexOfByteData{static_cast<uint16_t>(startIndex + (static_cast<uint16_t>(_streamSamplesPerBit) * 7U))};

  // validate if byte data fit into the buffer
  if (lastIndexOfByteData > LAST_STREAM_INDEX) {
    return {};
  }

  uint16_t bitIndex{startIndex};
  bool bitValueIsOne{initialBitValueIsOne};

  // get data from stream (MSb to LSb)
  uint8_t byteFromStream{0U};
  for (auto bitNo{0U}; bitNo < 8U; bitNo++) {
    // on 1st pass it doesn't harm the value as it is initialized to 0
    byteFromStream <<= 1U;

    // significant sample value mean there was a signal phase change thus bit value has changed
    if (isSampleValueOutOfNoiseRegion(bitIndex)) {
      bitValueIsOne = !bitValueIsOne;
    }

    // retrieve correct bit value
    byteFromStream |= (bitValueIsOne ? 0x01 : 0x00);

    // go ahead with next bit
    bitIndex += _streamSamplesPerBit;
  }

  // result include starting conditions for next byte retrieval
  return std::make_tuple(byteFromStream, bitIndex, bitValueIsOne);
}

bool DataDecoder::validateSyncWordLocationInStream(uint16_t syncWordStartIndex) {
  // get upper part of the sync word (it is also a frame start)
  auto syncWordPart{getByteFromStream(syncWordStartIndex, FRAME_DATA_READ_START_PRECONDITION)};
  if (not syncWordPart.has_value()) {
    return false;
  }

  uint16_t syncWordFromStream{0U};

  const auto [syncWordMsb, syncWordLsbStartIndex, syncWordLsbInitialBitValueIsOne] = syncWordPart.value();
  syncWordFromStream += syncWordMsb;
  syncWordFromStream <<= 8U;

  // get lower part of the sync word
  syncWordPart = getByteFromStream(syncWordLsbStartIndex, syncWordLsbInitialBitValueIsOne);
  if (not syncWordPart.has_value()) {
    return false;
  }

  const auto [syncWordLsb, notUsed1, notUsed2] = syncWordPart.value();
  syncWordFromStream += syncWordLsb;

  return (syncWordFromStream == SYNC_WORD) ? true : false;
}

std::optional<std::tuple<DataDecoder::TimeFrame, uint16_t>> DataDecoder::getTimeFrameDataFromStream(uint16_t dataStartIndex) {
  const auto samplesNoForTimeFrame{static_cast<uint16_t>(static_cast<uint16_t>(static_cast<uint16_t>(TIME_FRAME_BYTES_NO) * 8U * _streamSamplesPerBit) - _streamSamplesPerBit)};  // samples are spaced every _streamSamplesPerBit
  const auto lastIndexOfTheTimeFrame{static_cast<uint16_t>(dataStartIndex + samplesNoForTimeFrame)};

  // check if it is possible to extract required amount of data
  if (lastIndexOfTheTimeFrame > LAST_STREAM_INDEX) {
    return {};
  }

  // retrieve the data
  DataDecoder::TimeFrame dataFrame{};
  uint16_t byteStartIndex{dataStartIndex};
  bool startingBitValueIsOne{FRAME_DATA_READ_START_PRECONDITION};

  for (auto dataByteNo{0U}; dataByteNo < TIME_FRAME_BYTES_NO; dataByteNo++) {
    const auto dataByteGetter{getByteFromStream(byteStartIndex, startingBitValueIsOne)};

    if (not dataByteGetter.has_value()) {
#ifdef DEBUG
      printf("\nE: Can't get byte from stream starting at index: %d", byteStartIndex);
#endif
      return {};
    }

    const auto [dataByte, nextByteStartIndex, bitValueIsOne] = dataByteGetter.value();

    dataFrame.at(dataByteNo) = dataByte;
    byteStartIndex = nextByteStartIndex;
    startingBitValueIsOne = bitValueIsOne;
  }

  return std::make_tuple(dataFrame, lastIndexOfTheTimeFrame);
}

}  // namespace eczas
