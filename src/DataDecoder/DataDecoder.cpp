/**
 * @file DataDecoder.cpp
 * @author Grzegorz Kaczmarek SP6HFE
 * @brief
 * @version 0.1
 * @date 2024-10-07
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <DataDecoder/DataDecoder.hpp>
#include <CRC8/CRC8.hpp>

#include <cstdlib>
#include <stdint.h>
#include <optional>
#include <tuple>
#include <utility>

namespace eczas {

DataDecoder::DataDecoder(uint8_t streamSamplesPerBit) : _streamSamplesPerBit(streamSamplesPerBit) {
  _stream.fill(0);
  _correlator.fill(false);
  _sampleNo.fill(0U);
}

bool DataDecoder::processNewSample(int16_t sample) {
  static auto sampleNo{0U};
  static auto syncWordLookup{true};

  addNewData(sample, sampleNo);
  calculateSyncWordCorrelation();

  if (syncWordLookup and syncWordDetectedByCorrelation()) {
    syncWordLookup = false;
  }

  if (not syncWordLookup) {
    const auto nextTimeFrameStartIndexGetter{getTimeFrameDataFromStream()};

    if (nextTimeFrameStartIndexGetter.has_value()) {
      const auto timeFrameProcessingError{processTimeFrameData()};

      if (timeFrameProcessingError) {
        // currently extracted frame doesn't look like the one we are looking for - increase _meaningfulDataStartIndex by one
        _meaningfulDataStartIndex++;
      } else {
        // move stream meaningful data index beyond already extracted time frame (to prevent repeated detection)
        _meaningfulDataStartIndex = nextTimeFrameStartIndexGetter.value();
      }

      syncWordLookup = true;
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

void DataDecoder::registerRawTimeFrameCallback(TimeFrameCallback callback) {
  _rawTimeFrameCallback = std::move(callback);
}

void DataDecoder::registerRsProcessedTimeFrameCallback(TimeFrameCallback callback) {
  _rsProcessedTimeFrameCallback = std::move(callback);
}

void DataDecoder::registerCrcProcessedTimeFrameCallback(TimeFrameCallback callback) {
  _crcProcessedTimeFrameCallback = std::move(callback);
}

void DataDecoder::registerTimeFrameProcessingErrorCallback(TimeFrameProcessingErrorCallback callback) {
  _timeFrameProcessingErrorCallback = std::move(callback);
}

void DataDecoder::addNewData(int16_t sample, uint32_t sampleNo) {
  // TODO: redo using circular buffer

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
}

void DataDecoder::calculateSyncWordCorrelation() {
  /* Calculate correlation against 16 bit sync word 0x5555 (alternating bit values)
     - LSb of the sync word is the last sample in the stream buffer and should be 1,
     - sync word bit samples used in calculation are spaced in buffer with _streamSamplesPerBit,
     - MSb of the sync word is located (15 * _streamSamplesPerBit) bits back with respect to LSb sample,
     - correlation is placed at sync word's MSb index to ease further localization of the frame start,
     - carrier phase changes are expected to be cyclic (no value stalling causing stream sample value to be around 0),
     - stream samples representing sync word bits are expected to have significant values,

     Data frames are separated with some fill-up time so they can start at full second.
     Before beginning of the sync word stream values are around value 0 (no carrier phase change).
     Drop in stream's sample value below 0 (and lower hysteresis region) is an indication of the start of bit value 0 transmission.
     Jump in stream's sample value above 0 (and higher hysteresis region) is an indication of the start of bit value 1 transmission.
     Each time frame starts with an indication of the bit value 0 being transmitted.
     In order to detect where sync word 0x5555 lay in the stream a correlation estimate is calculated on each new signal sample recption. */

  bool correlationDetected{true};
  auto streamIndexToSyncWordBit{LAST_STREAM_INDEX};

  if (_stream[streamIndexToSyncWordBit] > 0) {  // validate potential value of the sync word's LSb
    for (uint8_t syncWordBitNo{0U}; syncWordBitNo < SYNC_WORD_BITS_NO; syncWordBitNo++) {
      if (not isSampleValueOutOfNoiseRegion(streamIndexToSyncWordBit)) {
        correlationDetected = false;
        break;
      }
      streamIndexToSyncWordBit -= _streamSamplesPerBit;
    }
  } else {
    // stream sample for sync word LSb wasn't indicating it may be 1
    correlationDetected = false;
  }

  // store correlation result into the buffer at sync word's start index
  const auto syncWordStartIndex{static_cast<uint16_t>(LAST_STREAM_INDEX - static_cast<uint16_t>(static_cast<uint16_t>(SYNC_WORD_BITS_NO - 1U) * _streamSamplesPerBit))};
  _correlator[syncWordStartIndex] = correlationDetected;
}

bool DataDecoder::isSampleValueOutOfNoiseRegion(uint16_t index) {
  if (index >= STREAM_SIZE) {
    // Sample index is out of range
    return false;
  }

  const auto sampleValue{_stream[index]};
  return (abs(sampleValue) > STREAM_NOISE_HYSTERESIS);
}

bool DataDecoder::syncWordDetectedByCorrelation() {
  const auto samplesNoForSyncWord{static_cast<uint16_t>(static_cast<uint16_t>(static_cast<uint16_t>(SYNC_WORD_BITS_NO - 1U) * _streamSamplesPerBit) + 1U)};
  const auto samplesNoWithoutCorrelationData{static_cast<uint16_t>(samplesNoForSyncWord - 1U)};  // correlation is calculated for sync word length backwards from newly added sample so for any newly added sample respective correlation is saved 15 bits (spaced every _streamSamplesPerBit) earlier at MSb index
  const auto startIndexOfNotCalculatedCorrelatorData{static_cast<uint16_t>(STREAM_SIZE - samplesNoWithoutCorrelationData)};

  // validate if it is worth doing any data analysis
  if (_meaningfulDataStartIndex >= startIndexOfNotCalculatedCorrelatorData) {
    return false;
  }

  auto syncWordDetected{false};
  auto syncWordStartIndex{0U};

  // using correlation array find index of the 1st detected sync word presence
  for (auto correlatorIndex{_meaningfulDataStartIndex}; correlatorIndex < startIndexOfNotCalculatedCorrelatorData; correlatorIndex++) {
    if (_correlator[correlatorIndex]) {
      syncWordDetected = true;
      syncWordStartIndex = correlatorIndex;
      break;
    }
  }

  // if sync word is not detected invalidate all acquired data (where correlation was estimated)
  if (not syncWordDetected) {
    _meaningfulDataStartIndex = startIndexOfNotCalculatedCorrelatorData;
    return false;
  }

  // 1st correlation index was found - all the data before syncWordStartIndex is not usable
  _meaningfulDataStartIndex = syncWordStartIndex;

  return true;
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

std::optional<uint16_t> DataDecoder::getTimeFrameDataFromStream() {
  const auto samplesNoForTimeFrame{static_cast<uint16_t>(static_cast<uint16_t>(static_cast<uint16_t>(TIME_FRAME_BYTES_NO) * 8U * _streamSamplesPerBit) - _streamSamplesPerBit)};  // samples are spaced every _streamSamplesPerBit

  // check if it is possible to extract required amount of data
  if (_meaningfulDataStartIndex > (STREAM_SIZE - samplesNoForTimeFrame)) {
    return {};
  }

  // retrieve the data
  uint16_t byteStartIndex{_meaningfulDataStartIndex};
  bool startingBitValueIsOne{FRAME_DATA_READ_START_PRECONDITION};

  for (auto dataByteNo{0U}; dataByteNo < TIME_FRAME_BYTES_NO; dataByteNo++) {
    const auto dataByteGetter{getByteFromStream(byteStartIndex, startingBitValueIsOne)};

    if (not dataByteGetter.has_value()) {
      // Can't get byte from the stream
      return {};
    }

    const auto [dataByte, nextByteStartIndex, bitValueIsOne] = dataByteGetter.value();

    _timeFrame.at(dataByteNo) = dataByte;
    byteStartIndex = nextByteStartIndex;
    startingBitValueIsOne = bitValueIsOne;
  }

  return byteStartIndex;
}

bool DataDecoder::processTimeFrameData() {
  static constexpr bool NO_ERROR{false};
  static constexpr bool AN_ERROR{true};

  if (validateTimeFrameStaticFields()) {
    return AN_ERROR;
  }

  // notify raw time frame extracted from the stream
  if (_rawTimeFrameCallback) {
    _rawTimeFrameCallback({_timeFrame, _sampleNo[_meaningfulDataStartIndex]});
  }

  if (correctTimeFrameErrorsWithRsFec()) {
    if (_timeFrameProcessingErrorCallback) {
      _timeFrameProcessingErrorCallback(TimeFrameProcessingError::RsCorrectionFailed);
    }
    return AN_ERROR;
  }

  // notify time frame with RS corrected time data
  if (_rsProcessedTimeFrameCallback) {
    _rsProcessedTimeFrameCallback({_timeFrame, _sampleNo[_meaningfulDataStartIndex]});
  }

  if (correctSk1ErrorWithCrc()) {
    // TODO: add option to not throw time frame away if transmitter state is not as important
    if (_timeFrameProcessingErrorCallback) {
      _timeFrameProcessingErrorCallback(TimeFrameProcessingError::CrcCorrectionFailed);
    }
    return AN_ERROR;
  }

  // notify time frame with CRC corrected SK1 bit
  if (_crcProcessedTimeFrameCallback) {
    _crcProcessedTimeFrameCallback({_timeFrame, _sampleNo[_meaningfulDataStartIndex]});
  }

  descrambleTimeMessage();
  extractTimeData();

  // notify time data
  if (_timeDataCallback) {
    _timeDataCallback({_timeData, _sampleNo[_meaningfulDataStartIndex]});
  }

  return NO_ERROR;
}

bool DataDecoder::validateTimeFrameStaticFields() {
  static constexpr bool NO_ERROR{false};
  static constexpr bool AN_ERROR{true};

  // validate synchronization word
  const auto frameSyncWordOk{(_timeFrame.at(0) == static_cast<uint8_t>(SYNC_WORD >> 8U)) and (_timeFrame.at(1) == static_cast<uint8_t>(SYNC_WORD & 0x00FF))};
  if (not frameSyncWordOk) {
    return AN_ERROR;
  }

  // validate time frame start byte
  const auto timeFrameStartByteOk{_timeFrame.at(2) == TIME_FRAME_START_BYTE};
  if (not timeFrameStartByteOk) {
    return AN_ERROR;
  }

  // validate time frame static bits (3 MSb of byte 3 is 0b101)
  const auto timeFrameStaticBitsOk{(static_cast<uint8_t>(_timeFrame.at(3) >> 5U) == TIME_MESSAGE_PREFIX)};
  if (not timeFrameStaticBitsOk) {
    return AN_ERROR;
  }

  return NO_ERROR;
}

bool DataDecoder::correctTimeFrameErrorsWithRsFec() {
  static constexpr bool NO_ERROR{false};
  static constexpr bool AN_ERROR{true};

  RS::Codeword codeword{};

  // lookup and correct time message (S0-SK0) errors using Reed-Solomon FEC data (ECC0-ECC2)

  // 1. Get codeword from the time frame
  {
    auto codewordIndex{0U};
    // not aligned bits S0-SK0
    for (auto frameByteNo{3U}; frameByteNo < 8U; frameByteNo++) {
      // get reminder of the previous symbol (stored in bits 7-5)
      if (frameByteNo != 3U) {
        codeword.at(codewordIndex++) += ((_timeFrame.at(frameByteNo) >> 5U) & 0x07);
      }
      // get full symbol in the middle of the byte (bits 4-1)
      codeword.at(codewordIndex++) = ((_timeFrame.at(frameByteNo) >> 1U) & 0x0F);
      // get MSb of the next symbol (stored in bit 0)
      if (frameByteNo != 7U) {
        codeword.at(codewordIndex) = ((_timeFrame.at(frameByteNo) & 0x01) << 3U);
      }
    }
    // aligned bits in bytes ECC0-ECC2
    for (auto frameByteNo{8U}; frameByteNo < 11U; frameByteNo++) {
      codeword.at(codewordIndex++) = ((_timeFrame.at(frameByteNo) >> 4U) & 0x0F);
      codeword.at(codewordIndex++) = (_timeFrame.at(frameByteNo) & 0x0F);
    }
  }

  // 2. Recover possibly faulty codeword
  const auto recoveryError{_rs.recoverCodeword(codeword)};
  if (recoveryError) {
    return AN_ERROR;
  }

  // 3. Update the time frame with corrected data
  {
    auto codewordIndex{0U};
    // not aligned bits S0-SK0
    for (auto frameByteNo{3U}; frameByteNo < 8U; frameByteNo++) {
      uint8_t updatedFrameByte{0U};
      // set 3 LSb reminder of the current symbol (on 1st pass keep original 3 MSb)
      if (frameByteNo == 3U) {
        updatedFrameByte = (_timeFrame.at(frameByteNo) &= 0xE0);
      } else {
        updatedFrameByte = ((codeword.at(codewordIndex++) & 0x07) << 5U);
      }
      // set full symbol in the middle of the byte (bits 4-1)
      updatedFrameByte |= (codeword.at(codewordIndex++) << 1U);
      // set MSb of the next symbol as LSb of the time frame byte (preserve time frame LSb in byte 7)
      if (frameByteNo == 7U) {
        updatedFrameByte |= (_timeFrame.at(frameByteNo) &= 0x01);
      } else {
        updatedFrameByte |= ((codeword.at(codewordIndex) & 0x08) >> 3U);
      }
      _timeFrame.at(frameByteNo) = updatedFrameByte;
    }
    // aligned bits in bytes ECC0-ECC2
    for (auto frameByteNo{8U}; frameByteNo < 11U; frameByteNo++) {
      _timeFrame.at(frameByteNo) = (codeword.at(codewordIndex++) << 4U);
      _timeFrame.at(frameByteNo) |= codeword.at(codewordIndex++);
    }
  }

  return NO_ERROR;
}

void DataDecoder::descrambleTimeMessage() {
  // descramble time message (37 bytes starting at byte 3 bit 4 until byte 7 bit 0; 3 MSb of scrambling word are 0 (0x0A) so they won't affect message's static part)
  auto timeFrameByteNo{3U};
  for (const auto scramblingByte : _scramblingWord) {
    _timeFrame.at(timeFrameByteNo++) ^= scramblingByte;
  }
}

void DataDecoder::extractTimeData() {
  for (auto timeFrameByteNo{3U}; timeFrameByteNo < 8U; timeFrameByteNo++) {
    // at the same time grab timestamp data
    switch (timeFrameByteNo) {
      case 3U:  // 5 LSb only
        _timeData.utcTimestamp = (_timeFrame.at(timeFrameByteNo) & 0x1F);
        break;
      case 7U:  // MSb only
        _timeData.utcTimestamp <<= 1U;
        _timeData.utcTimestamp += ((_timeFrame.at(timeFrameByteNo) & 0x80) ? 1U : 0U);
        break;
      default:  // full byte
        _timeData.utcTimestamp <<= 8U;
        _timeData.utcTimestamp += _timeFrame.at(timeFrameByteNo);
        break;
    }
  }

  // correct received timestamp as it means the number of 3[s] periods since beginning of the year 2000
  static constexpr uint32_t secondsBetweenYear1970And2000{946684800U};

  _timeData.utcTimestamp *= 3U;
  _timeData.utcUnixTimestamp = _timeData.utcTimestamp + secondsBetweenYear1970And2000;

  // get the local time offset (bits TZ0 (6) and TZ1 (5)) - this should be sent other way around for simpler decoding
  const auto timeOffset{static_cast<uint8_t>((_timeFrame.at(7U) >> 5) & 0x03)};
  switch (timeOffset) {
    case 0x01:
      _timeData.offset = TimeZoneOffset::OffsetPlus2h;
      break;
    case 0x02:
      _timeData.offset = TimeZoneOffset::OffsetPlus1h;
      break;
    case 0x03:
      _timeData.offset = TimeZoneOffset::OffsetPlus3h;
      break;
    default:
      _timeData.offset = TimeZoneOffset::OffsetPlus0h;
      break;
  }

  // get time zone change announcement (bit TZC(2))
  _timeData.timeZoneChangeAnnouncement = ((static_cast<uint8_t>(_timeFrame.at(7U) >> 2U) & 0x01) ? true : false);

  // extract leap second related information (bits LS(4) and LSS(3))
  _timeData.leapSecondAnnounced = ((static_cast<uint8_t>(_timeFrame.at(7U) >> 4U) & 0x01) ? true : false);
  _timeData.leapSecondPositive = ((static_cast<uint8_t>(_timeFrame.at(7U) >> 3U) & 0x01) ? true : false);

  // extract transmitter state (bits SK0 (1) and SK1 (0)) - this should be sent other way around for simpler decoding
  const auto transmitterState{static_cast<uint8_t>(_timeFrame.at(7U) & 0x03)};
  switch (transmitterState) {
    case 0x01:
      _timeData.transmitterState = TransmitterState::PlannedMaintenance1Week;
      break;
    case 0x02:
      _timeData.transmitterState = TransmitterState::PlannedMaintenance1Day;
      break;
    case 0x03:
      _timeData.transmitterState = TransmitterState::PlannedMaintenanceOver1Week;
      break;
    default:
      _timeData.transmitterState = TransmitterState::NormalOperation;
      break;
  }
}

bool DataDecoder::validateCrc() {
  static constexpr bool NO_ERROR{false};
  static constexpr bool AN_ERROR{true};

  // Time frame byte 11 contain CRC8 hash calculated over data bytes 3-7

  crc::CRC8 crc{CRC8_POLYNOMIAL, CRC8_INIT_VALUE};

  for (auto byteNo{3U}; byteNo < 8U; byteNo++) {
    crc.update(_timeFrame.at(byteNo));
  }

  if ((crc.get() != _timeFrame.at(11))) {
    return AN_ERROR;
  }

  return NO_ERROR;
}

bool DataDecoder::correctSk1ErrorWithCrc() {
  static constexpr bool NO_ERROR{false};
  static constexpr bool AN_ERROR{true};

  /* After successful time frame data retrieval with Reed-Solomon the only data bit left, not covered with FEC, is SK1.
     Out of time frame bytes 3-7 the only unknown information is SK1 (0x101 in byte 3 is static and validated already).
     CRC8 may be calculated from data with SK1 bit value as received and also with its value being flipped.
     When CRC-8 byte (11th) of the time frame wasn't corrupted SK1 may be recovered using mentioned checks.
     In case of SK1 retrieval failure it is to be decided by the app if whole time frame should be discarded or the transmitter state
     should be marked as unknown (SK0-SK1). */

  crc::CRC8 crc{CRC8_POLYNOMIAL, CRC8_INIT_VALUE};

  // 1. Validate received CRC against the time frame data (3-7) as is
  if (not validateCrc()) {
    return NO_ERROR;
  }

  // 2. If no success flip SK1 (LSb) bit and check again
  _timeFrame.at(7) ^= 0x01;
  if (not validateCrc()) {
    return NO_ERROR;
  }

  // 3. If still no success revert SK1 value to leave the timeFrame in an original form
  _timeFrame.at(7) ^= 0x01;

  return AN_ERROR;
}

}  // namespace eczas
