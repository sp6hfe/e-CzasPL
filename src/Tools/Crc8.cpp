#include <Tools/Crc8.hpp>

namespace tools {

Crc8::Crc8(uint8_t polynomial, uint8_t initValue) : _polynomial(polynomial), _initValue(initValue) {}

void Crc8::init() {
  _crc = _initValue;
}

void Crc8::update(const uint8_t data) {
  // step 1: XOR with the data value
  _crc ^= data;

  // step 2: shift left 8 times and on each stage if MSb is 1 then XOR with polynomial
  for (auto bitNo{8U}; bitNo > 0; --bitNo) {
    if (_crc & 0x80) {
      _crc = ((_crc << 1U) ^ _polynomial);
    } else {
      _crc <<= 1U;
    }
  }
}

uint8_t Crc8::getCrc8() {
  return _crc;
}

}  // namespace tools