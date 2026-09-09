#include "twinvq_mdct.hpp"

#include <cmath>

namespace twinvq {

constexpr float kPi = 3.14159265358979323846f;

void imdct_half(float* output, const float* input, int ncoeffs, float scale) {
    // Direct IMDCT. imdct_half[i] = full[N/2 + i] for i in 0..N-1,
    // full[n] = sum_k X[k] cos(pi/N (n + 1/2 + N/2)(k + 1/2)).
    const int N = ncoeffs;
    const double pi_over_n = 3.14159265358979323846 / static_cast<double>(N);
    for (int i = 0; i < N; i++) {
        const double a = (static_cast<double>(N) + i + 0.5) * pi_over_n;
        double c0 = std::cos(a * 0.5);
        double c1 = std::cos(a * 1.5);
        const double w = 2.0 * std::cos(a);
        double sum = static_cast<double>(input[0]) * c0 + static_cast<double>(input[1]) * c1;
        for (int k = 2; k < N; k++) {
            const double c2 = w * c1 - c0;
            sum += static_cast<double>(input[k]) * c2;
            c0 = c1;
            c1 = c2;
        }
        output[i] = static_cast<float>(sum * static_cast<double>(scale));
    }
}

void sine_window(float* dst, int n) {
    for (int i = 0; i < n; i++)
        dst[i] = std::sin((static_cast<float>(i) + 0.5f) * (kPi / (2.0f * static_cast<float>(n))));
}

void vector_fmul(float* dst, const float* src0, const float* src1, int len) {
    for (int i = 0; i < len; i++)
        dst[i] = src0[i] * src1[i];
}

void vector_fmul_window(float* dst, const float* src0, const float* src1, const float* win, int len) {
    dst += len;
    win += len;
    src0 += len;
    for (int i = -len, j = len - 1; i < 0; i++, j--) {
        const float s0 = src0[i];
        const float s1 = src1[j];
        const float wi = win[i];
        const float wj = win[j];
        dst[i] = s0 * wj - s1 * wi;
        dst[j] = s0 * wi + s1 * wj;
    }
}

void butterflies(float* left, float* right, int len) {
    for (int i = 0; i < len; i++) {
        const float a = left[i];
        const float b = right[i];
        left[i] = a + b;
        right[i] = a - b;
    }
}

} // namespace twinvq
