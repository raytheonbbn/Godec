#pragma once

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath> 
#include <godec/HelperFuncs.h>

namespace Godec {

class LinearResample {
 public:
  LinearResample(int32_t samp_rate_in_hz,
                 int32_t samp_rate_out_hz,
                 float filter_cutoff_hz,
                 int32_t num_zeros);

  void Resample(const Vector &input,
                bool flush,
                Vector *output);

  void Reset();
 private:
  int64_t GetNumOutputSamples(int64_t input_num_samp, bool flush) const;
  inline void GetIndexes(int64_t samp_out,
                         int64_t *first_samp_in,
                         int32_t *samp_out_wrapped) const;

  void SetRemainder(const Vector &input);

  void SetIndexesAndWeights();

  float FilterFunc(float) const;

  // The following variables are provided by the user.
  int32_t samp_rate_in_;
  int32_t samp_rate_out_;
  float filter_cutoff_;
  int32_t num_zeros_;

  int32_t input_samples_in_unit_;   ///< The number of input samples in the
                                  ///< smallest repeating unit: num_samp_in_ =
                                  ///< samp_rate_in_hz / Gcd(samp_rate_in_hz,
                                  ///< samp_rate_out_hz)
  int32_t output_samples_in_unit_;  ///< The number of output samples in the
                                  ///< smallest repeating unit: num_samp_out_ =
                                  ///< samp_rate_out_hz / Gcd(samp_rate_in_hz,
                                  ///< samp_rate_out_hz)


  std::vector<int32_t> first_index_;
  std::vector<Vector> weights_;
  int64_t input_sample_offset_;  ///< The number of input samples we have
                               ///< already received for this signal
                               ///< (including anything in remainder_)
  int64_t output_sample_offset_;  ///< The number of samples we have already
                                ///< output for this signal.
  Vector input_remainder_;  ///< A small trailing part of the
                                       ///< previously seen input signal.
};

}

