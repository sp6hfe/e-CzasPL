#pragma once

#include <stdint.h>

namespace reedsolomon {

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
class ReedSolomon {
public:
  static constexpr size_t codewordSize{(1U << bitspersymbol) - 1U};
  static constexpr size_t fecSize{2U * AmountOfCorrectableSymbols};

  static_assert(BitsPerSymbol != 0U, "A symbol should consist of at least 1 bit of data");
  static_assert((codewordSize - fecSize) >= AmountOfCorrectableSymbols, "Can't fit FEC data allowing to correct requested amount of errorneous symbols");

  static constexpr size_t dataSize{codewordSize - fecSize};

  size_t getSymbolSize() const { return _bitsPerSymbol; }

  size_t getCodewordSize() const { return codewordSize; }

  size_t getDataSize() const { return dataSize; }

  size_t getFecSize() const { return fecSize; }

  void generate_gf();

  void gen_poly();

  void encode_rs();

  void decode_rs();

  ReedSolomon() : _bitsPerSymbol(BitsPerSymbol), _correctableSymbols(AmountOfCorrectableSymbols){};

private:
  /* Variables introduced for the wrapper */
  uint8_t _bitsPerSymbol;
  uint8_t _correctableSymbols;

  /* Variables used by rs.cpp */
  int _alpha[codewordSize];
  int _index[codewordSize];
  int _g[codewordSize];

  int alpha_to[codewordSize + 1U];
  int index_of[codewordSize + 1U];
  int gg[fecSize + 1U];
  int recd[codewordSize];
  int data[dataSize];
  int bb[fecSize];
};

}  // namespace reedsolomon
