#pragma once

#include <stdint.h>

namespace tools {

class Crc8 {
public:
  /**
   * @brief Constructor
   *
   * @param initValue Algorithm initialization value
   */
  Crc8(uint8_t polynomial, uint8_t initValue);

  void init();

  void update(const uint8_t data);

  uint8_t getCrc8();

  /// @brief Default destructor
  ~Crc8() = default;

private:
  uint8_t _polynomial;

  uint8_t _initValue;

  uint8_t _crc{0U};
};

}  // namespace tools
