#pragma once

namespace twinvq {

// Inverse MDCT "half": writes ncoeffs samples equal to the middle half of a
// 2*ncoeffs-point IMDCT. ncoeffs must be a power of two in 32..2048.
void imdct_half(float* output, const float* input, int ncoeffs, float scale);

// Compare FFT IMDCT against the direct cosine definition. Returns true on pass.
bool imdct_self_test(float* max_abs_err);

void sine_window(float* dst, int n);
const float* sine_window_cached(int n);

void vector_fmul(float* dst, const float* src0, const float* src1, int len);

void vector_fmul_window(float* dst, const float* src0, const float* src1, const float* win, int len);

} // namespace twinvq
