#pragma once

#include <functional>
#include <limits>
#include <stdint.h>
#include <array>
#include <optional>
#include <tuple>

namespace eczas {

/// @brief eCzasPL time data decoder
class DataDecoder {
public:
  /// @brief Size of the stream buffer
  static constexpr uint16_t STREAM_SIZE{1024U};  // should cover at least one complete frame + at least 2*streamSamplesPerBit (to properly detect frame start)

  /// @brief Last index number in stream buffer
  static constexpr uint16_t LAST_STREAM_INDEX{STREAM_SIZE - 1U};

  /// @brief +/- region to treat stream sample value as noise
  static constexpr uint16_t STREAM_NOISE_HYSTERESIS{15000};

  /// @brief Data frame synchronization word
  static constexpr uint16_t SYNC_WORD{0x5555};  // arbitrary value

  /// @brief Data frame synchronization length in bits
  static constexpr uint8_t SYNC_WORD_BITS_NO{16U};  // arbitrary value

  /// @brief Value indicating sync word start index was detected
  static constexpr int32_t SYNC_WORD_START_DETECTED{std::numeric_limits<int32_t>::max()};  // well above possible correlation value

  /// @brief Correlation estimate value treated as valid
  static constexpr int32_t CORRELATION_VALID{static_cast<int32_t>(STREAM_NOISE_HYSTERESIS) * 30U};  // MSB within hysteresis (0) + 15 * at least hysteresis value taken twice

  /// @brief Correlation estimate value treated as invalid
  static constexpr int32_t CORRELATION_INVALID{std::numeric_limits<int32_t>::min()};  // well below achievable value from correlation calculation

  /// @brief Initial value to correclty retrieve frame data from stream
  static constexpr bool FRAME_DATA_READ_START_PRECONDITION{true};

  /// @brief Time frame lenght in bytes
  static constexpr uint8_t TIME_FRAME_BYTES_NO{12U};  // arbitrary value

  /// @brief Time frame start byte
  static constexpr uint8_t TIME_FRAME_START_BYTE{0x60};

  /// @brief Time frame static bits
  static constexpr uint8_t TIME_FRAME_STATIC_BITS{0xA0};

  /// @brief Time frame static bits mask
  static constexpr uint8_t TIME_FRAME_STATIC_BITS_MASK{0xE0};

  /// @brief Time frame data container
  using TimeFrame = std::array<uint8_t, TIME_FRAME_BYTES_NO>;

  /// @brief Time frame reception callback (frame, frame start sample no)
  using TimeFrameCallback = std::function<void(std::pair<const TimeFrame&, uint32_t>)>;

  /**
   * @brief Constructor
   *
   * @param streamSamplesPerBit Amount of samples per signal bit
   */
  explicit DataDecoder(uint8_t streamSamplesPerBit);

  /// @brief Default destructor
  ~DataDecoder() = default;

  /**
   * @brief Register time frame reception callback
   *
   * @param callback The callback
   */
  void registerTimeFrameReceptionCallback(TimeFrameCallback callback);

  /**
   * @brief Process new sample
   * @note Adds sample to internal buffer and calculate sync word correlation.
   *       Looks up for new frames and extract them.
   *
   * @param sample The sample
   * @return true Internal buffer got full (new data gets lost)
   * @return false There is a room for new samples to process
   */
  bool processNewSample(int16_t sample);

private:
  std::array<int16_t, STREAM_SIZE> _stream{};

  std::array<int32_t, STREAM_SIZE> _correlator{};

  std::array<uint32_t, STREAM_SIZE> _sampleNo{};

  TimeFrameCallback _timeFrameCallback{nullptr};

  uint8_t _streamSamplesPerBit;

  uint16_t _meaningfulDataStartIndex{STREAM_SIZE};

  void calculateSyncWordCorrelation();

  bool isSampleValueOutOfNoiseRegion(uint16_t index);

  std::optional<uint16_t> detectSyncWordStartIndexByCorrelation();

  std::optional<uint16_t> lookupFrameStartIndex();

  std::optional<std::tuple<uint8_t, uint16_t, bool>> getByteFromStream(uint16_t startIndex, bool initialBitValueIsOne);

  bool validateSyncWordLocationInStream(uint16_t syncWordStartIndex);

  std::optional<std::tuple<TimeFrame, uint16_t>> getTimeFrameDataFromStream(uint16_t dataStartIndex);
};

}  // namespace eczas