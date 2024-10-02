/* This program is an encoder/decoder for Reed-Solomon codes. Encoding is in
   systematic form, decoding via the Berlekamp iterative algorithm.
   In the present form , the constants mm, nn, tt, and kk=nn-2tt must be
   specified  (the double letters are used simply to avoid clashes with
   other n,k,t used in other programs into which this was incorporated!)
   Also, the irreducible polynomial used to generate GF(2**mm) must also be
   entered -- these can be found in Lin and Costello, and also Clark and Cain.

   The representation of the elements of GF(2**m) is either in index form,
   where the number is the power of the primitive element alpha, which is
   convenient for multiplication (add the powers modulo 2**m-1) or in
   polynomial form, where the bits represent the coefficients of the
   polynomial representation of the number, which is the most convenient form
   for addition.  The two forms are swapped between via lookup tables.
   This leads to fairly messy looking expressions, but unfortunately, there
   is no easy alternative when working with Galois arithmetic.

   The code is not written in the most elegant way, but to the best
   of my knowledge, (no absolute guarantees!), it works.
   However, when including it into a simulation program, you may want to do
   some conversion of global variables (used here because I am lazy!) to
   local variables where appropriate, and passing parameters (eg array
   addresses) to the functions  may be a sensible move to reduce the number
   of global variables and thus decrease the chance of a bug being introduced.

   This program does not handle erasures at present, but should not be hard
   to adapt to do this, as it is just an adjustment to the Berlekamp-Massey
   algorithm. It also does not attempt to decode past the BCH bound -- see
   Blahut "Theory and practice of error control codes" for how to do this.

              Simon Rockliff, University of Adelaide   21/9/89

   26/6/91 Slight modifications to remove a compiler dependent bug which hadn't
           previously surfaced. A few extra comments added for clarity.
           Appears to all work fine, ready for posting to net!

                  Notice
                 --------
   This program may be freely modified and/or given to whoever wants it.
   A condition of such distribution is that the author's contribution be
   acknowledged by his name being left in the comments heading the program,
   however no responsibility is accepted for any financial or other loss which
   may result from some unforseen errors or malfunctioning of the program
   during use.
                                 Simon Rockliff, 26th June 1991

  Original code adopted to become a templated C++ class allowing for effective
  compile-time parametrization of the Reed-Solomon encoder/decoder.
                                 Grzegorz Kaczmarek, SP6HFE, 2024
*/

#pragma once

#include <math.h>
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

  size_t getSymbolSize() const { return BitsPerSymbol; }

  size_t getCodewordSize() const { return codewordSize; }

  size_t getDataSize() const { return dataSize; }

  size_t getFecSize() const { return fecSize; }

  void generate_gf();

  void gen_poly();

  void encode_rs();

  void decode_rs();

  ReedSolomon();

private:
  // Variable naming made similar as in original code

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
};

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
ReedSolomon<BitsPerSymbol, AmountOfCorrectableSymbols>::ReedSolomon() {
  // Initialize polynomial coefficients depending on template non-type parameter BitsPerSymbol
  uint8_t index{0U};

  if constexpr (BitsPerSymbol == 2U) {
    /* 1 + x + x^2 */
    for (auto newValue : {1, 1, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 3) {
    /* 1 + x + x^3 */
    for (auto newValue : {1, 1, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 4) {
    /* 1 + x + x^4 */
    for (auto newValue : {1, 1, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 5) {
    /* 1 + x^2 + x^5 */
    for (auto newValue : {1, 0, 1, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 6) {
    /* 1 + x + x^6 */
    for (auto newValue : {1, 1, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 7) {
    /* 1 + x^3 + x^7 */
    for (auto newValue : {1, 0, 0, 1, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 8) {
    /* 1 + x^2 + x^3 + x^4 + x^8 */
    for (auto newValue : {1, 0, 1, 1, 1, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 9) {
    /* 1 + x^4 + x^9 */
    for (auto newValue : {1, 0, 0, 0, 1, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 10) {
    /* 1 + x^3 + x^10 */
    for (auto newValue : {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 11) {
    /* 1 + x^2 + x^11 */
    for (auto newValue : {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 12) {
    /* 1 + x + x^4 + x^6 + x^12 */
    for (auto newValue : {1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 13) {
    /* 1 + x + x^3 + x^4 + x^13 */
    for (auto newValue : {1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 14) {
    /* 1 + x + x^6 + x^10 + x^14 */
    for (auto newValue : {1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 15) {
    /* 1 + x + x^15 */
    for (auto newValue : {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  } else if constexpr (BitsPerSymbol == 16) {
    /* 1 + x + x^3 + x^12 + x^16 */
    for (auto newValue : {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1}) {
      pp[index++] = newValue;
    }
  }
}

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
void ReedSolomon<BitsPerSymbol, AmountOfCorrectableSymbols>::generate_gf()
/* generate GF(2**mm) from the irreducible polynomial p(X) in pp[0]..pp[mm]
   lookup tables:  index->polynomial form   alpha_to[] contains j=alpha**i;
                   polynomial form -> index form  index_of[j=alpha**i] = i
   alpha=2 is the primitive element of GF(2**mm)
*/
{
  int i, mask;

  mask = 1;
  alpha_to[BitsPerSymbol] = 0;
  for (i = 0; i < BitsPerSymbol; i++) {
    alpha_to[i] = mask;
    index_of[alpha_to[i]] = i;
    if (pp[i] != 0)
      alpha_to[BitsPerSymbol] ^= mask;
    mask <<= 1;
  }
  index_of[alpha_to[BitsPerSymbol]] = BitsPerSymbol;
  mask >>= 1;
  for (i = BitsPerSymbol + 1; i < codewordSize; i++) {
    if (alpha_to[i - 1] >= mask)
      alpha_to[i] = alpha_to[BitsPerSymbol] ^ ((alpha_to[i - 1] ^ mask) << 1);
    else
      alpha_to[i] = alpha_to[i - 1] << 1;
    index_of[alpha_to[i]] = i;
  }
  index_of[0] = -1;
}

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
void ReedSolomon<BitsPerSymbol, AmountOfCorrectableSymbols>::gen_poly()
/* Obtain the generator polynomial of the tt-error correcting, length
  nn=(2**mm -1) Reed Solomon code  from the product of (X+alpha**i), i=1..2*tt
*/
{
  int i, j;

  gg[0] = 2; /* primitive element alpha = 2  for GF(2**mm)  */
  gg[1] = 1; /* g(x) = (X+alpha) initially */
  for (i = 2; i <= codewordSize - dataSize; i++) {
    gg[i] = 1;
    for (j = i - 1; j > 0; j--)
      if (gg[j] != 0)
        gg[j] = gg[j - 1] ^ alpha_to[(index_of[gg[j]] + i) % codewordSize];
      else
        gg[j] = gg[j - 1];
    gg[0] = alpha_to[(index_of[gg[0]] + i) % codewordSize]; /* gg[0] can never be zero */
  }
  /* convert gg[] to index form for quicker encoding */
  for (i = 0; i <= codewordSize - dataSize; i++) gg[i] = index_of[gg[i]];
}

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
void ReedSolomon<BitsPerSymbol, AmountOfCorrectableSymbols>::encode_rs()
/* take the string of symbols in data[i], i=0..(k-1) and encode systematically
   to produce 2*tt parity symbols in bb[0]..bb[2*tt-1]
   data[] is input and bb[] is output in polynomial form.
   Encoding is done by using a feedback shift register with appropriate
   connections specified by the elements of gg[], which was generated above.
   Codeword is   c(X) = data(X)*X**(nn-kk)+ b(X)          */
{
  int i, j;
  int feedback;

  for (i = 0; i < codewordSize - dataSize; i++) bb[i] = 0;
  for (i = dataSize - 1; i >= 0; i--) {
    feedback = index_of[data[i] ^ bb[codewordSize - dataSize - 1]];
    if (feedback != -1) {
      for (j = codewordSize - dataSize - 1; j > 0; j--)
        if (gg[j] != -1)
          bb[j] = bb[j - 1] ^ alpha_to[(gg[j] + feedback) % codewordSize];
        else
          bb[j] = bb[j - 1];
      bb[0] = alpha_to[(gg[0] + feedback) % codewordSize];
    } else {
      for (j = codewordSize - dataSize - 1; j > 0; j--)
        bb[j] = bb[j - 1];
      bb[0] = 0;
    };
  };
};

template <uint8_t BitsPerSymbol, uint8_t AmountOfCorrectableSymbols>
void ReedSolomon<BitsPerSymbol, AmountOfCorrectableSymbols>::decode_rs()
/* assume we have received bits grouped into mm-bit symbols in recd[i],
   i=0..(nn-1),  and recd[i] is index form (ie as powers of alpha).
   We first compute the 2*tt syndromes by substituting alpha**i into rec(X) and
   evaluating, storing the syndromes in s[i], i=1..2tt (leave s[0] zero) .
   Then we use the Berlekamp iteration to find the error location polynomial
   elp[i].   If the degree of the elp is >tt, we cannot correct all the errors
   and hence just put out the information symbols uncorrected. If the degree of
   elp is <=tt, we substitute alpha**i , i=1..n into the elp to get the roots,
   hence the inverse roots, the error location numbers. If the number of errors
   located does not equal the degree of the elp, we have more than tt errors
   and cannot correct them.  Otherwise, we then solve for the error value at
   the error location and correct the error.  The procedure is that found in
   Lin and Costello. For the cases where the number of errors is known to be too
   large to correct, the information symbols as received are output (the
   advantage of systematic encoding is that hopefully some of the information
   symbols will be okay and that if we are in luck, the errors are in the
   parity part of the transmitted codeword).  Of course, these insoluble cases
   can be returned as error flags to the calling routine if desired.   */
{
  int i, j, u, q;
  int elp[codewordSize - dataSize + 2][codewordSize - dataSize], d[codewordSize - dataSize + 2], l[codewordSize - dataSize + 2], u_lu[codewordSize - dataSize + 2], s[codewordSize - dataSize + 1];
  int count = 0, syn_error = 0, root[AmountOfCorrectableSymbols], loc[AmountOfCorrectableSymbols], z[AmountOfCorrectableSymbols + 1], err[codewordSize], reg[AmountOfCorrectableSymbols + 1];

  /* first form the syndromes */
  for (i = 1; i <= codewordSize - dataSize; i++) {
    s[i] = 0;
    for (j = 0; j < codewordSize; j++)
      if (recd[j] != -1)
        s[i] ^= alpha_to[(recd[j] + i * j) % codewordSize]; /* recd[j] in index form */
                                                            /* convert syndrome from polynomial form to index form  */
    if (s[i] != 0) syn_error = 1;                           /* set flag if non-zero syndrome => error */
    s[i] = index_of[s[i]];
  };

  if (syn_error) /* if errors, try and correct */
  {
    /* compute the error location polynomial via the Berlekamp iterative algorithm,
       following the terminology of Lin and Costello :   d[u] is the 'mu'th
       discrepancy, where u='mu'+1 and 'mu' (the Greek letter!) is the step number
       ranging from -1 to 2*tt (see L&C),  l[u] is the
       degree of the elp at that step, and u_l[u] is the difference between the
       step number and the degree of the elp.
    */
    /* initialise table entries */
    d[0] = 0;      /* index form */
    d[1] = s[1];   /* index form */
    elp[0][0] = 0; /* index form */
    elp[1][0] = 1; /* polynomial form */
    for (i = 1; i < codewordSize - dataSize; i++) {
      elp[0][i] = -1; /* index form */
      elp[1][i] = 0;  /* polynomial form */
    }
    l[0] = 0;
    l[1] = 0;
    u_lu[0] = -1;
    u_lu[1] = 0;
    u = 0;

    do {
      u++;
      if (d[u] == -1) {
        l[u + 1] = l[u];
        for (i = 0; i <= l[u]; i++) {
          elp[u + 1][i] = elp[u][i];
          elp[u][i] = index_of[elp[u][i]];
        }
      } else
      /* search for words with greatest u_lu[q] for which d[q]!=0 */
      {
        q = u - 1;
        while ((d[q] == -1) && (q > 0)) q--;
        /* have found first non-zero d[q]  */
        if (q > 0) {
          j = q;
          do {
            j--;
            if ((d[j] != -1) && (u_lu[q] < u_lu[j]))
              q = j;
          } while (j > 0);
        };

        /* have now found q such that d[u]!=0 and u_lu[q] is maximum */
        /* store degree of new elp polynomial */
        if (l[u] > l[q] + u - q)
          l[u + 1] = l[u];
        else
          l[u + 1] = l[q] + u - q;

        /* form new elp(x) */
        for (i = 0; i < codewordSize - dataSize; i++) elp[u + 1][i] = 0;
        for (i = 0; i <= l[q]; i++)
          if (elp[q][i] != -1)
            elp[u + 1][i + u - q] = alpha_to[(d[u] + codewordSize - d[q] + elp[q][i]) % codewordSize];
        for (i = 0; i <= l[u]; i++) {
          elp[u + 1][i] ^= elp[u][i];
          elp[u][i] = index_of[elp[u][i]]; /*convert old elp value to index*/
        }
      }
      u_lu[u + 1] = u - l[u + 1];

      /* form (u+1)th discrepancy */
      if (u < codewordSize - dataSize) /* no discrepancy computed on last iteration */
      {
        if (s[u + 1] != -1)
          d[u + 1] = alpha_to[s[u + 1]];
        else
          d[u + 1] = 0;
        for (i = 1; i <= l[u + 1]; i++)
          if ((s[u + 1 - i] != -1) && (elp[u + 1][i] != 0))
            d[u + 1] ^= alpha_to[(s[u + 1 - i] + index_of[elp[u + 1][i]]) % codewordSize];
        d[u + 1] = index_of[d[u + 1]]; /* put d[u+1] into index form */
      }
    } while ((u < codewordSize - dataSize) && (l[u + 1] <= AmountOfCorrectableSymbols));

    u++;
    if (l[u] <= AmountOfCorrectableSymbols) /* can correct error */
    {
      /* put elp into index form */
      for (i = 0; i <= l[u]; i++) elp[u][i] = index_of[elp[u][i]];

      /* find roots of the error location polynomial */
      for (i = 1; i <= l[u]; i++)
        reg[i] = elp[u][i];
      count = 0;
      for (i = 1; i <= codewordSize; i++) {
        q = 1;
        for (j = 1; j <= l[u]; j++)
          if (reg[j] != -1) {
            reg[j] = (reg[j] + j) % codewordSize;
            q ^= alpha_to[reg[j]];
          };
        if (!q) /* store root and error location number indices */
        {
          root[count] = i;
          loc[count] = codewordSize - i;
          count++;
        };
      };
      if (count == l[u]) /* no. roots = degree of elp hence <= tt errors */
      {
        /* form polynomial z(x) */
        for (i = 1; i <= l[u]; i++) /* Z[0] = 1 always - do not need */
        {
          if ((s[i] != -1) && (elp[u][i] != -1))
            z[i] = alpha_to[s[i]] ^ alpha_to[elp[u][i]];
          else if ((s[i] != -1) && (elp[u][i] == -1))
            z[i] = alpha_to[s[i]];
          else if ((s[i] == -1) && (elp[u][i] != -1))
            z[i] = alpha_to[elp[u][i]];
          else
            z[i] = 0;
          for (j = 1; j < i; j++)
            if ((s[j] != -1) && (elp[u][i - j] != -1))
              z[i] ^= alpha_to[(elp[u][i - j] + s[j]) % codewordSize];
          z[i] = index_of[z[i]]; /* put into index form */
        };

        /* evaluate errors at locations given by error location numbers loc[i] */
        for (i = 0; i < codewordSize; i++) {
          err[i] = 0;
          if (recd[i] != -1) /* convert recd[] to polynomial form */
            recd[i] = alpha_to[recd[i]];
          else
            recd[i] = 0;
        }
        for (i = 0; i < l[u]; i++) /* compute numerator of error term first */
        {
          err[loc[i]] = 1; /* accounts for z[0] */
          for (j = 1; j <= l[u]; j++)
            if (z[j] != -1)
              err[loc[i]] ^= alpha_to[(z[j] + j * root[i]) % codewordSize];
          if (err[loc[i]] != 0) {
            err[loc[i]] = index_of[err[loc[i]]];
            q = 0; /* form denominator of error term */
            for (j = 0; j < l[u]; j++)
              if (j != i)
                q += index_of[1 ^ alpha_to[(loc[j] + root[i]) % codewordSize]];
            q = q % codewordSize;
            err[loc[i]] = alpha_to[(err[loc[i]] - q + codewordSize) % codewordSize];
            recd[loc[i]] ^= err[loc[i]]; /*recd[i] must be in polynomial form */
          }
        }
      } else                               /* no. roots != degree of elp => >tt errors and cannot solve */
        for (i = 0; i < codewordSize; i++) /* could return error flag if desired */
          if (recd[i] != -1)               /* convert recd[] to polynomial form */
            recd[i] = alpha_to[recd[i]];
          else
            recd[i] = 0;                 /* just output received codeword as is */
    } else                               /* elp has degree has degree >tt hence cannot solve */
      for (i = 0; i < codewordSize; i++) /* could return error flag if desired */
        if (recd[i] != -1)               /* convert recd[] to polynomial form */
          recd[i] = alpha_to[recd[i]];
        else
          recd[i] = 0; /* just output received codeword as is */
  } else               /* no non-zero syndromes => no errors: output received codeword */
    for (i = 0; i < codewordSize; i++)
      if (recd[i] != -1) /* convert recd[] to polynomial form */
        recd[i] = alpha_to[recd[i]];
      else
        recd[i] = 0;
}

}  // namespace reedsolomon
