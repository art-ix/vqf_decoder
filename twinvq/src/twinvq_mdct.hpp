#pragma once

namespace twinvq {

// Inverse MDCT "half" matching FFmpeg ff_imdct_half:
// nbits such that n = 1<<nbits = 2*ncoeffs; writes ncoeffs samples.
// scale is multiplied into the rotation constants.
void imdct_half(float* output, const float* input, int ncoeffs, float scale);

void sine_window(float* dst, int n);

void vector_fmul(float* dst, const float* src0, const float* src1, int len);

void vector_fmul_window(float* dst, const float* src0, const float* src1, const float* win, int len);

void butterflies(float* left, float* right, int len);

} // namespace twinvq
