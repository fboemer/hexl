// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

#include <limits>
#include <vector>

#include "hexl/util/check.hpp"
#include "hexl/util/compiler.hpp"

namespace intel {
namespace hexl {

/// @brief Pre-computes a Barrett factor with which modular multiplication can
/// be performed more efficiently
class MultiplyFactor {
 public:
  MultiplyFactor() = default;

  /// @brief Computes and stores the Barrett factor floor((operand << bit_shift)
  /// / modulus). This is useful when modular multiplication of the form
  /// (x * operand) mod modulus is performed with same modulus and operand
  /// several times. Note, passing operand=1 can be used to pre-compute a
  /// Barrett factor for multiplications of the form (x * y) mod modulus, where
  /// only the modulus is re-used across calls to modular multiplication.
  MultiplyFactor(uint64_t operand, uint64_t bit_shift, uint64_t modulus)
      : m_operand(operand) {
    HEXL_CHECK(operand <= modulus, "operand " << operand
                                              << " must be less than modulus "
                                              << modulus);
    HEXL_CHECK(bit_shift == 32 || bit_shift == 52 || bit_shift == 64,
               "Unsupported BitShift " << bit_shift);
    uint64_t op_hi = operand >> (64 - bit_shift);
    uint64_t op_lo = (bit_shift == 64) ? 0 : (operand << bit_shift);

    m_barrett_factor = DivideUInt128UInt64Lo(op_hi, op_lo, modulus);
  }

  /// @brief Returns the pre-computed Barrett factor
  inline uint64_t BarrettFactor() const { return m_barrett_factor; }

  /// @brief Returns the operand corresponding to the Barrett factor
  inline uint64_t Operand() const { return m_operand; }

 private:
  uint64_t m_operand;
  uint64_t m_barrett_factor;
};

/// @brief Returns whether or not num is a power of two
inline bool IsPowerOfTwo(uint64_t num) { return num && !(num & (num - 1)); }

/// @brief Returns floor(log2(x))
inline uint64_t Log2(uint64_t x) { return MSB(x); }

inline bool IsPowerOfFour(uint64_t num) {
  return IsPowerOfTwo(num) && (Log2(num) % 2 == 0);
}

/// @brief Returns the maximum value that can be represented using \p bits bits
inline uint64_t MaximumValue(uint64_t bits) {
  HEXL_CHECK(bits <= 64, "MaximumValue requires bits <= 64; got " << bits);
  if (bits == 64) {
    return (std::numeric_limits<uint64_t>::max)();
  }
  return (1ULL << bits) - 1;
}

/// @brief Reverses the bits
/// @param[in] x Input to reverse
/// @param[in] bit_width Number of bits in the input; must be >= MSB(x)
/// @return The bit-reversed representation of \p x using \p bit_width bits
uint64_t ReverseBits(uint64_t x, uint64_t bit_width);

/// @brief Returns x^{-1} mod modulus
/// @details Requires x % modulus != 0
uint64_t InverseMod(uint64_t x, uint64_t modulus);

/// @brief Returns (x * y) mod modulus
/// @details Assumes x, y < modulus
uint64_t MultiplyMod(uint64_t x, uint64_t y, uint64_t modulus);

/// @brief Returns (x * y) mod modulus
/// @param[in] x
/// @param[in] y
/// @param[in] y_precon 64-bit precondition factor floor(2**64 / modulus)
/// @param[in] modulus
uint64_t MultiplyMod(uint64_t x, uint64_t y, uint64_t y_precon,
                     uint64_t modulus);

/// @brief Returns (x + y) mod modulus
/// @details Assumes x, y < modulus
uint64_t AddUIntMod(uint64_t x, uint64_t y, uint64_t modulus);

/// @brief Returns (x - y) mod modulus
/// @details Assumes x, y < modulus
uint64_t SubUIntMod(uint64_t x, uint64_t y, uint64_t modulus);

/// @brief Returns base^exp mod modulus
uint64_t PowMod(uint64_t base, uint64_t exp, uint64_t modulus);

/// @brief Returns whether or not root is a degree-th root of unity mod modulus
/// @param[in] root Root of unity to check
/// @param[in] degree Degree of root of unity; must be a power of two
/// @param[in] modulus Modulus of finite field
bool IsPrimitiveRoot(uint64_t root, uint64_t degree, uint64_t modulus);

/// @brief Tries to return a primitive degree-th root of unity
/// @details Returns 0 or throws an error if no root is found
uint64_t GeneratePrimitiveRoot(uint64_t degree, uint64_t modulus);

/// @brief Returns whether or not root is a degree-th root of unity
/// @param[in] degree Must be a power of two
/// @param[in] modulus Modulus of finite field
uint64_t MinimalPrimitiveRoot(uint64_t degree, uint64_t modulus);

/// @brief Computes (x * y) mod modulus, except that the output is in [0, 2 *
/// modulus]
/// @param[in] x
/// @param[in] y_operand also denoted y
/// @param[in] modulus
/// @param[in] y_barrett_factor Pre-computed Barrett reduction factor floor((y
/// << BitShift) / modulus)
template <int BitShift>
inline uint64_t MultiplyModLazy(uint64_t x, uint64_t y_operand,
                                uint64_t y_barrett_factor, uint64_t modulus) {
  HEXL_CHECK(y_operand < modulus, "y_operand " << y_operand
                                               << " must be less than modulus "
                                               << modulus);
  HEXL_CHECK(
      modulus <= MaximumValue(BitShift),
      "Modulus " << modulus << " exceeds bound " << MaximumValue(BitShift));
  HEXL_CHECK(x <= MaximumValue(BitShift),
             "Operand " << x << " exceeds bound " << MaximumValue(BitShift));

  uint64_t Q = MultiplyUInt64Hi<BitShift>(x, y_barrett_factor);
  return y_operand * x - Q * modulus;
}

/// @brief Computes (x * y) mod modulus, except that the output is in [0, 2 *
/// modulus]
/// @param[in] x
/// @param[in] y
/// @param[in] modulus
template <int BitShift>
inline uint64_t MultiplyModLazy(uint64_t x, uint64_t y, uint64_t modulus) {
  HEXL_CHECK(BitShift == 64 || BitShift == 52,
             "Unsupported BitShift " << BitShift);
  HEXL_CHECK(x <= MaximumValue(BitShift),
             "Operand " << x << " exceeds bound " << MaximumValue(BitShift));
  HEXL_CHECK(y < modulus,
             "y " << y << " must be less than modulus " << modulus);
  HEXL_CHECK(
      modulus <= MaximumValue(BitShift),
      "Modulus " << modulus << " exceeds bound " << MaximumValue(BitShift));

  uint64_t y_barrett = MultiplyFactor(y, BitShift, modulus).BarrettFactor();
  return MultiplyModLazy<BitShift>(x, y, y_barrett, modulus);
}

/// @brief Adds two unsigned 64-bit integers
/// @param operand1 Number to add
/// @param operand2 Number to add
/// @param result Stores the sum
/// @return The carry bit
inline unsigned char AddUInt64(uint64_t operand1, uint64_t operand2,
                               uint64_t* result) {
  *result = operand1 + operand2;
  return static_cast<unsigned char>(*result < operand1);
}

/// @brief Returns whether or not the input is prime
bool IsPrime(uint64_t n);

/// @brief Generates a list of num_primes primes in the range [2^(bit_size),
// 2^(bit_size+1)]. Ensures each prime q satisfies
// q % (2*ntt_size+1)) == 1
/// @param[in] num_primes Number of primes to generate
/// @param[in] bit_size Bit size of each prime
/// @param[in] prefer_small_primes When true, returns primes starting from
/// 2^(bit_size); when false, returns primes starting from 2^(bit_size+1)
/// @param[in] ntt_size N such that each prime q satisfies q % (2N) == 1. N must
/// be a power of two less than 2^bit_size.
std::vector<uint64_t> GeneratePrimes(size_t num_primes, size_t bit_size,
                                     bool prefer_small_primes,
                                     size_t ntt_size = 1);

/// @brief Returns input mod modulus, computed via 64-bit Barrett reduction
/// @param[in] input
/// @param[in] modulus
/// @param[in] q_barr floor(2^64 / modulus)
inline uint64_t BarrettReduce64(uint64_t input, uint64_t modulus,
                                uint64_t q_barr) {
  HEXL_CHECK(modulus != 0, "modulus == 0");
  uint64_t q = MultiplyUInt64Hi<64>(input, q_barr);
  uint64_t q_times_input = input - q * modulus;
  return q_times_input >= modulus ? q_times_input - modulus : q_times_input;
}

/// @brief Returns x mod modulus, assuming x < InputModFactor * modulus
/// @param[in] x
/// @param[in] modulus also denoted q
/// @param[in] twice_modulus 2 * q; must not be nullptr if InputModFactor == 4
/// or 8
/// @param[in] four_times_modulus 4 * q; must not be nullptr if InputModFactor
/// == 8
template <int InputModFactor>
uint64_t ReduceMod(uint64_t x, uint64_t modulus,
                   const uint64_t* twice_modulus = nullptr,
                   const uint64_t* four_times_modulus = nullptr) {
  HEXL_CHECK(InputModFactor == 1 || InputModFactor == 2 ||
                 InputModFactor == 4 || InputModFactor == 8,
             "InputModFactor should be 1, 2, 4, or 8");
  if (InputModFactor == 1) {
    return x;
  }
  if (InputModFactor == 2) {
    if (x >= modulus) {
      x -= modulus;
    }
    return x;
  }
  if (InputModFactor == 4) {
    HEXL_CHECK(twice_modulus != nullptr, "twice_modulus should not be nullptr");
    if (x >= *twice_modulus) {
      x -= *twice_modulus;
    }
    if (x >= modulus) {
      x -= modulus;
    }
    return x;
  }
  if (InputModFactor == 8) {
    HEXL_CHECK(twice_modulus != nullptr, "twice_modulus should not be nullptr");
    HEXL_CHECK(four_times_modulus != nullptr,
               "four_times_modulus should not be nullptr");

    if (x >= *four_times_modulus) {
      x -= *four_times_modulus;
    }
    if (x >= *twice_modulus) {
      x -= *twice_modulus;
    }
    if (x >= modulus) {
      x -= modulus;
    }
    return x;
  }
  HEXL_CHECK(false, "Should be unreachable");
  return x;
}

}  // namespace hexl
}  // namespace intel
