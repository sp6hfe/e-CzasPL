#pragma once

#include <stdint.h>
#include <stdio.h>

namespace tools {

class Helpers {
public:
  static void printBinaryValue(uint8_t value) {
    for (auto bit{0U}; bit < 8U; bit++) {
      const auto bitValueOne{(value & 0x80) ? true : false};
      if (bitValueOne) {
        printf("1");
      } else {
        printf("0");
      }
      value <<= 1U;
    }
  }
};

}  // namespace tools
