#pragma once

#include <stddef.h>
#include <stdint.h>

namespace reedsolomon {

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
class ReedSolomon {
public:
  static constexpr size_t codewordSize{(1U << BitsPerSymbol) - 1U};
  static constexpr size_t fecSize{2U * AmountOfCorrectableSymbols};

  static_assert(BitsPerSymbol >= 2U, "A symbol should consist of at least 2 bits of data");
  static_assert(BitsPerSymbol <= 16U, "A symbol should consist of at most 16 bits of data");
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

  ReedSolomon();

private:
  uint8_t _bitsPerSymbol;
  uint8_t _correctableSymbols;

  /* Variables used by original code */
  int _alpha[codewordSize];
  int _index[codewordSize];
  int _g[codewordSize];

  int alpha_to[codewordSize + 1U];
  int index_of[codewordSize + 1U];
  int gg[fecSize + 1U];
  int recd[codewordSize];
  int data[dataSize];
  int bb[fecSize];
  int pp[BitsPerSymbol + 1];

  /* Primitive polynomials - see Lin & Costello, Appendix A,
   * and  Lee & Messerschmitt, p. 453.
   */
  // #if (BitsPerSymbol == 2)
  //   int pp[BitsPerSymbol + 1] = {1, 1, 1};
  // #elif (BitsPerSymbol == 3)
  //   /* 1 + x + x^3 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 1};
  // #elif (BitsPerSymbol == 4)
  //   /* 1 + x + x^4 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 0, 1};
  // #elif (BitsPerSymbol == 5)
  //   /* 1 + x^2 + x^5 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 1, 0, 0, 1};
  // #elif (BitsPerSymbol == 6)
  //   /* 1 + x + x^6 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 7)
  //   /* 1 + x^3 + x^7 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 0, 1, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 8)
  //   /* 1+x^2+x^3+x^4+x^8 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 1, 1, 1, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 9)
  //   /* 1+x^4+x^9 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 10)
  //   /* 1+x^3+x^10 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 11)
  //   /* 1+x^2+x^11 */
  //   int pp[BitsPerSymbol + 1] = {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 12)
  //   /* 1+x+x^4+x^6+x^12 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 13)
  //   /* 1+x+x^3+x^4+x^13 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 14)
  //   /* 1+x+x^6+x^10+x^14 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 15)
  //   /* 1+x+x^15 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  // #elif (BitsPerSymbol == 16)
  //   /* 1+x+x^3+x^12+x^16 */
  //   int pp[BitsPerSymbol + 1] = {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1};
  // #else
  // #error "BitsPerSymbol is out of range (2-16)"
  // #endif
};

}  // namespace reedsolomon
