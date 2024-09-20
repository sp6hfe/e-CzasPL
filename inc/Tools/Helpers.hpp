#pragma once

#include <stdint.h>
#include <stdio.h>

namespace tools {

class Helpers {
public:
  static void printBinaryValuePart(uint8_t value, bool upperPart) {
    if (not upperPart) {
      value <<= 4U;
    }

    for (auto bitCounter{0U}; bitCounter < 4U; bitCounter++) {
      const auto bitValueOne{(value & 0x80) ? true : false};
      if (bitValueOne) {
        printf("1");
      } else {
        printf("0");
      }
      value <<= 1U;
    }
  }

  static void printBinaryValue(uint8_t value) {
    printBinaryValuePart(value, true);
    printBinaryValuePart(value, false);
  }
};

}  // namespace tools
