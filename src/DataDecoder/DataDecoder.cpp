#include <DataDecoder/DataDecoder.hpp>

#include <cstdlib>
#include <stdint.h>
#include <stdio.h>
#include <optional>
#include <tuple>
#include <utility>

namespace eczas {

DataDecoder::DataDecoder(uint8_t streamSamplesPerBit) : _streamSamplesPerBit(streamSamplesPerBit) {
  _stream.fill(0);
  _correlator.fill(CORRELATION_INVALID);
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
  _correlator[LAST_STREAM_INDEX] = 0;
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

      // Check CRC-8 - to be discussed with GUM

      // Lookup and correct errors (Reed-Solomon) - to be discussed with GUM

      // validate time frame start byte
      const auto frameStartByteOk{timeFrame.at(2) == TIME_FRAME_START_BYTE};

      // validate time frame static bits
      const auto frameStaticBitsOk{TIME_FRAME_STATIC_BITS == (timeFrame.at(3) & TIME_FRAME_STATIC_BITS_MASK)};

      // descramble the time message
      if (frameStartByteOk and frameStaticBitsOk) {
        // _timeFrameCallback({timeFrame, _sampleNo[frameStartIndex.value()]});

        // descramble time message (37 bytes starting at byte 3 bit 4 until byte 7 bit 0)
        static constexpr uint8_t offsetToScramblingWordMsb{3U};  // 3 most significant bits of the 1st byte of the scrambling word array are useless (they are just a fill for the byte-type storage)
        auto timeFrameByteNo{3U};
        auto timeFrameByteBitNo{4U};
        auto timeFrameDataByte{timeFrame.at(timeFrameByteNo)};
        for (auto scramblingCounter{offsetToScramblingWordMsb}; scramblingCounter < (37U + offsetToScramblingWordMsb); scramblingCounter++) {
          const auto scramblingByteNo{static_cast<uint8_t>(scramblingCounter / 8U)};
          const auto scramblingByteBitNo{static_cast<uint8_t>(7U - (scramblingCounter % 8U))};
          const auto scramblingBit{static_cast<uint8_t>((_scramblingWord.at(scramblingByteNo) >> scramblingByteBitNo) & 0x01)};
          timeFrameDataByte ^= (scramblingBit << timeFrameByteBitNo);

          if (timeFrameByteBitNo == 0U) {
            timeFrame.at(timeFrameByteNo) = timeFrameDataByte;
            timeFrameByteNo++;
            timeFrameDataByte = timeFrame.at(timeFrameByteNo);
            timeFrameByteBitNo = 7U;
          } else {
            timeFrameByteBitNo--;
          }
        }
      }

      // communicate new data
      if (frameStartByteOk and frameStaticBitsOk and _timeFrameCallback) {
        _timeFrameCallback({timeFrame, _sampleNo[frameStartIndex.value()]});
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

void DataDecoder::registerTimeFrameReceptionCallback(TimeFrameCallback callback) {
  _timeFrameCallback = std::move(callback);
}

void DataDecoder::calculateSyncWordCorrelation() {
  /* Calculate correlation against 16 bit sync word 0x5555
     - LSb of the sync word is the last sample in the stream buffer
     - sync word bit samples used in calculation are spaced in buffer with _streamSamplesPerBit
     - MSb of the sync word is located (15 * _streamSamplesPerBit) bits back with respect to LSB sample
     - correlation is placed at sync word MSb index to ease further localization of the frame start

     Data frames are separated with some fill-up time so they can start at full second.
     Before beginning of the sync word stream values are around value 0 (no carrier phase change).
     MSb of sync word is not detectable from the stream values due to no phase change observed (but 0-value is usable later on).
     Drop in stream's sample value below 0 and hysteresis region is an indication of the start of bit value 0 transmission.
     Jump in stream's sample value above 0 and hysteresis gerion is an indication of the start of bit value 1 transmission.
     In order to precisely detect where sync word 0x5555 lay in the stream a correlation estimate is calculated for each new signal sample.
     Correlation estimator is a sum of stream sample values (making up potential sync word) according to expected pattern.
     Within expected region of bit 0 values are taken as they are whereas for regions where expected bit is 1 a sign of the sample is inverted
     (making it positive to increase correlation estimate value).
     To further increase sync word detection capabilities a hysteresis region is defined which is used to evaluate if the sample value
     lay within noise region or outside of it. Each sample in calculation, when outside of the noise region, is summed twice.
     MSb sample is also evaluated if falls below lower hysteresis region (meaning sync word starts from 0) and if not correlation is set as not valid. */

  static constexpr int16_t SYNC_WORD_BIT_0_CORRELATION_MULTIPLIER{-1};  // samples for bit value 0 should go negative so we need to invert their sign to increase correlation result
  static constexpr int16_t SYNC_WORD_BIT_1_CORRELATION_MULTIPLIER{1};   // samples for bit value 1 should go positive so we take their values directly into correlation result

  {
    // MSb sample should be the 1st one to fall below lower hysteresis as bit value 0 starts sync word transmission
    const auto streamIndexToSyncWordMsb{LAST_STREAM_INDEX - static_cast<uint16_t>(static_cast<uint16_t>(SYNC_WORD_BITS_NO - 1U) * _streamSamplesPerBit)};
    const auto msbIsBit0{isSampleValueOutOfNoiseRegion(streamIndexToSyncWordMsb) and (_stream[streamIndexToSyncWordMsb] < 0)};

    if (not msbIsBit0) {
      _correlator[streamIndexToSyncWordMsb] = CORRELATION_INVALID;
      return;
    }
  }

  int32_t correlation{0};

  // traverse stream to correlate according to 16 points spaced equally (every _streamSamplesPerBit)
  auto streamIndexToSyncWordBit{LAST_STREAM_INDEX};

  /* due to sync word value (alternating bit values) carrier phase changes are cyclic - this should give alternating sample values signs;
     sync word LSb is located at stream end, more significant bits are accessed by jumping back by _streamSamplesPerBit */
  for (uint8_t syncWordBitNo{0U}; syncWordBitNo < SYNC_WORD_BITS_NO; syncWordBitNo++) {
    const auto syncWordBitValueIs1{((SYNC_WORD >> syncWordBitNo) & static_cast<uint16_t>(0x1)) != 0U};
    const auto syncWordBitCorrelationMultiplier{syncWordBitValueIs1 ? SYNC_WORD_BIT_1_CORRELATION_MULTIPLIER : SYNC_WORD_BIT_0_CORRELATION_MULTIPLIER};
    const auto syncWordBitFromStream{_stream[streamIndexToSyncWordBit]};
    const auto syncWordBitCorrelation{static_cast<int32_t>(static_cast<int32_t>(syncWordBitFromStream) * syncWordBitCorrelationMultiplier)};
    correlation += syncWordBitCorrelation;

    // when sample is out of hysteresis it means it doesn't belong to the noise region and correlation is updated again (to strenghten estimator value change)
    if (isSampleValueOutOfNoiseRegion(streamIndexToSyncWordBit)) {
      correlation += syncWordBitCorrelation;
    }

    // to reach next sample a jump back is needed
    streamIndexToSyncWordBit -= _streamSamplesPerBit;
  }

  // update an index to the MSb
  streamIndexToSyncWordBit += _streamSamplesPerBit;

  /* store calculated correlation into the buffer at index where sync word beginning (MSB) is located;
     last index shift moved it to the place where MSb is located (tested against being within noise region already)*/
  _correlator[streamIndexToSyncWordBit] = correlation;

  return;
}

bool DataDecoder::isSampleValueOutOfNoiseRegion(uint16_t index) {
  if (index >= STREAM_SIZE) {
    printf("\nE: Sample index is out of range");
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

  uint16_t firstValidCorrelationIndex{0U};
  uint16_t samplesNoAllowedUntilNextValidCorrelation{0U};

  bool globalCorrelationMaxSearchOngoing{false};
  bool localCorrelationMaxSearchOngoing{false};
  uint16_t correlationMaxIndex{0U};
  int32_t correlationMaxValue{0};

  const auto startIndexOfNotCalculatedCorrelatorData{static_cast<uint16_t>(STREAM_SIZE - samplesNoWithoutCorrelationData)};
  const auto minSamplesNoForSyncWordDetection{static_cast<uint16_t>(samplesNoForSyncWord + samplesNoOfCorrelationMaxRepetition)};

  // validate if it is worth doing any data analysis; at least sync word + some extra should fit in the buffer (having corelation value calculated)
  if (_meaningfulDataStartIndex > (startIndexOfNotCalculatedCorrelatorData - minSamplesNoForSyncWordDetection)) {
    return {};
  }

  // using correlations array data find 1st best match for the sync word presence or latest found (valid) correlation max
  for (auto correlatorIndex{_meaningfulDataStartIndex}; correlatorIndex < startIndexOfNotCalculatedCorrelatorData; correlatorIndex++) {
    const auto correlatorValue{_correlator[correlatorIndex]};

    if (correlatorValue >= CORRELATION_VALID) {
      localCorrelationMaxSearchOngoing = true;
      // update on greater value
      if (correlatorValue > correlationMaxValue) {
        correlationMaxIndex = correlatorIndex;
        correlationMaxValue = correlatorValue;
      }

      // handle 1st valid correlation value (global search flag goes true once per detection round)
      if (not globalCorrelationMaxSearchOngoing) {
        globalCorrelationMaxSearchOngoing = true;
        firstValidCorrelationIndex = correlatorIndex;

        // validate if sync word start was detected already (and we basically wait for frame remaining data)
        if (correlatorValue == SYNC_WORD_START_DETECTED) {
          // best match was found already
          syncWordDetected = true;
          break;
        }
      }
    } else {  // this branch is for: correlatorValue < CORRELATION_VALID
      // looking for the 1st valid correlation value in the buffer
      if (not globalCorrelationMaxSearchOngoing) {
        continue;
      }

      // waiting allowed amount of samples until next local valid correlation region appear
      if (not localCorrelationMaxSearchOngoing) {
        // validate if to keep waiting until next valid correlation region (valid correlation areas from the same sync word appear periodically)
        if (--samplesNoAllowedUntilNextValidCorrelation > 0U) {
          continue;
        }

        // when reached here sync word best math was found already (no other valid correlation since samplesNoOfCorrelationMaxRepetition)
        _correlator[correlationMaxIndex] = SYNC_WORD_START_DETECTED;
        syncWordDetected = true;
        break;
      }

      // when correlation drops below valid value start waiting for next valid region (may appear if sync word best match was not reached yet)
      localCorrelationMaxSearchOngoing = false;
      samplesNoAllowedUntilNextValidCorrelation = samplesNoOfCorrelationMaxRepetition;
    }
  }

  // no matter if sync word best match was found all the data before correlationMaxIndex is not usable (frame data always starts from correlation max index, anything before is garbage)
  if (firstValidCorrelationIndex != correlationMaxIndex) {
    _meaningfulDataStartIndex = correlationMaxIndex;
  } else {
    // when all correlation values were not valid no data is usable
    if (firstValidCorrelationIndex == 0U) {
      _meaningfulDataStartIndex = startIndexOfNotCalculatedCorrelatorData;
    }
  }

  if (not syncWordDetected) {
    // couldn't detect with currently acquired data
    return {};
  }

  return correlationMaxIndex;
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
      printf("\nE: Can't get byte from stream starting at index: %d", byteStartIndex);
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
