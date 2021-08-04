// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

#include "eltwise/eltwise-cmp-sub-mod-avx512.hpp"
#include "eltwise/eltwise-cmp-sub-mod-internal.hpp"
#include "hexl/eltwise/eltwise-cmp-sub-mod.hpp"
#include "hexl/logging/logging.hpp"
#include "hexl/util/aligned-allocator.hpp"
#include "util/cpu-features.hpp"

namespace intel {
namespace hexl {

//=================================================================

// state[0] is the degree
static void BM_EltwiseCmpSubModNative(benchmark::State& state) {  //  NOLINT
  size_t input_size = state.range(0);

  std::random_device rd;
  std::mt19937 gen(rd());

  uint64_t modulus = 100;
  std::uniform_int_distribution<uint64_t> distrib(1, modulus - 1);

  uint64_t bound = distrib(gen);
  uint64_t diff = distrib(gen);
  AlignedVector64<uint64_t> input1(input_size);
  for (size_t i = 0; i < input_size; ++i) {
    input1[i] = distrib(gen);
  }

  for (auto _ : state) {
    EltwiseCmpSubModNative(input1.data(), input1.data(), input_size, modulus,
                           CMPINT::NLT, bound, diff);
  }
}

//=================================================================

#ifdef HEXL_HAS_AVX512DQ
// state[0] is the degree
static void BM_EltwiseCmpSubModAVX512(benchmark::State& state) {  //  NOLINT
  size_t input_size = state.range(0);
  size_t modulus = 100;

  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<uint64_t> distrib(1, modulus - 1);

  uint64_t bound = distrib(gen);
  uint64_t diff = distrib(gen);
  AlignedVector64<uint64_t> input1(input_size);
  for (size_t i = 0; i < input_size; ++i) {
    input1[i] = distrib(gen);
  }

  for (auto _ : state) {
    EltwiseCmpSubModAVX512(input1.data(), input1.data(), input_size, modulus,
                           CMPINT::NLT, bound, diff);
  }
}
#endif

//=================================================================

void register_eltwise_cmp_sub_mod_benchmarks() {
  benchmark::RegisterBenchmark("BM_EltwiseCmpSubModNative",
                               BM_EltwiseCmpSubModNative)
      ->Unit(benchmark::kMicrosecond)
      ->Args({1024})
      ->Args({4096})
      ->Args({16384});

#ifdef HEXL_HAS_AVX512DQ
  if (has_avx512dq) {
    benchmark::RegisterBenchmark("BM_EltwiseCmpSubModAVX512",
                                 BM_EltwiseCmpSubModAVX512)
        ->Unit(benchmark::kMicrosecond)
        ->Args({1024})
        ->Args({4096})
        ->Args({16384});
  }
#endif  // HEXL_HAS_AVX512DQ
}

}  // namespace hexl
}  // namespace intel
