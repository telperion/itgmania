#include "../SlapFFT.h"

#include <iomanip>
#include <ios>
#include <iostream>

#include "gtest/gtest.h"

namespace Slap {
namespace {

TEST(SlapFFTDiagnosticTest, Teapot) {
    EXPECT_EQ(7 * 6, 42);
}

TEST(SlapFFTConfigTest, Getters) {
    FFTConfiguration config(
        44100,
        32767,
        6,
        3,
        false
    );

    EXPECT_EQ(config.sample_rate(), 44100);
    EXPECT_EQ(config.max_src_signal(), 32767);
    EXPECT_EQ(config.size_p2(), 6);
    EXPECT_EQ(config.rate_reduction(), 3);
    EXPECT_EQ(config.invert(), false);

    // Length of the filter window should be 2^(size_p2+1) + 1.
    EXPECT_EQ(config.length(), (2 << 6) + 1);

    // Do a couple time/index conversions. These should disregard the rate reduction.
    EXPECT_EQ(config.index(10), 441000);
    EXPECT_EQ(config.time(220500), 5);
}

TEST(SlapFFTConfigTest, CheckWindowCalculation) {
    FFTConfiguration config(
        44100,
        32767,
        3,
        3,
        false
    );

    std::stringstream ss_frequency, ss_window;
    ss_frequency << std::fixed << std::setprecision(3);
    ss_window << std::fixed << std::setprecision(3);
    for (int i = 0; i < config.frequency().size(); ++i) {
        ss_frequency << "\t" << i << " " << config.frequency()[i] << std::endl;
    }
    for (int i = 0; i < config.window().size(); ++i) {
        ss_window << "\t" << i << " " << config.window()[i] << std::endl;
    }
    std::cout << "Frequency axis: " << std::endl << ss_frequency.str();
    std::cout << "Window: " << std::endl << ss_window.str();

    // Endpoints of the frequency axis should be 0 (the DC).
    EXPECT_NEAR(config.frequency().front(), 0.0, 1e-12);
    EXPECT_NEAR(config.frequency().back(), 0.0, 1e-12);

    // Midpoint of the frequency axis should be the Nyquist frequency
    // (half the sample rate) divided by the rate reduction.
    // e.g., 44100 Hz / 2 Nyquist / 3 r.r. = 7350 Hz
    EXPECT_NEAR(
        config.frequency()[config.frequency().size() / 2], 
        config.sample_rate() / 2.0 / config.rate_reduction(), 
        1e-12
    ); 

    // Endpoints of the Hann window should be zero.
    EXPECT_NEAR(config.window().front(), 0.0, 1e-12);
    EXPECT_NEAR(config.window().back(), 0.0, 1e-12);

    // Midpoint of the Hann window should be unity.
    EXPECT_NEAR(config.window()[config.window().size() / 2], 1.0, 1e-12);
}

TEST(SlapFFTTest, Getters) {
    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(0);
    }

    FFTConfiguration config(
        44100,
        32767,
        6,
        3,
        false
    );

    FFT dut(
        src,
        config,
        10
    );

    EXPECT_EQ(dut.cache_size_p2(), 10);
    EXPECT_EQ(dut.cache_size_max(), 1 << 10);
    EXPECT_EQ(dut.cache_size(), 0);
    EXPECT_EQ(dut.src().size(), 300);
    EXPECT_EQ(dut.config().max_src_signal(), 32767);
}

TEST(SlapFFTTest, NoSignal) {
    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(0);
    }

    FFTConfiguration config(
        44100,
        32767,
        4,
        3,
        false
    );

    FFT dut(
        src,
        config,
        10
    );

    Signal dst;
    dut.fft(dst, src.size() / 2);

    std::stringstream ss_fft;
    ss_fft << std::fixed << std::setprecision(3);
    for (int i = 0; i < dst.size(); ++i) {
        ss_fft << "\t" << i << " " << dut.config().frequency()[i] << " -> " << std::abs(dst[i]) << std::endl;
    }
    std::cout << "Result of FFT: " << std::endl << ss_fft.str();

    for (int i = 0; i < dst.size(); ++i) {
        EXPECT_NEAR(std::abs(dst[i]), 0.0, 1e-12);
    }
}

TEST(SlapFFTTest, OnlyConstant) {
    float constant_signal = 10000.0;
    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(constant_signal);
    }

    FFTConfiguration config(
        44100,
        32767,
        4,
        3,
        false
    );

    FFT dut(
        src,
        config,
        10
    );

    Signal dst;
    dut.fft(dst, src.size() / 2);

    std::stringstream ss_fft;
    ss_fft << std::fixed << std::setprecision(3);
    for (int i = 0; i < dst.size(); ++i) {
        ss_fft << "\t" << i << " " << dut.config().frequency()[i] << " -> " << std::abs(dst[i]) << std::endl;
    }
    std::cout << "Result of FFT: " << std::endl << ss_fft.str();

    // The frequency = 0 Hz tap should be strong,
    // but there should be little to no other response.
    EXPECT_NEAR(std::abs(dst[0]), constant_signal * (1 << dut.config().size_p2()), constant_signal * 1e-3);
    for (int i = 4; i < dst.size()-4; ++i) {
        EXPECT_NEAR(std::abs(dst[i]), 0.0, constant_signal * 1e-3);
    }
}

TEST(SlapFFTTest, OneFrequencyNoRateReduction) {
    FFTConfiguration config(
        44100,
        32767,
        4,
        1,
        false
    );
    float test_frequency = 5500.0;
    float test_period = config.sample_rate() / test_frequency;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(32767 * std::sinf(TAU * i / test_period));
    }

    FFT dut(
        src,
        config,
        10
    );

    Signal dst;
    dut.fft(dst, src.size() / 2);

    std::stringstream ss_fft;
    ss_fft << std::fixed << std::setprecision(3);
    for (int i = 0; i < dst.size(); ++i) {
        ss_fft << "\t" << i << " " << dut.config().frequency()[i] << " -> " << std::abs(dst[i]) << std::endl;
    }
    std::cout << "Result of FFT: " << std::endl << ss_fft.str();

    // The maximum response should be at the nearest frequency tap.
    // Check single-endedly -> dst.size() / 2 as the inclusive limit.
    int closest_frequency_index = 0;
    float closest_frequency = 0.0f;
    int highest_response_index = 0;
    float highest_response = 0.0f;
    for (int i = 0; i <= dst.size() / 2; ++i) {
        float frequency_i = dut.config().frequency()[i];
        float response_i = std::abs(dst[i]);
        if (std::abs(frequency_i - test_frequency) < std::abs(closest_frequency - test_frequency)) {
            closest_frequency_index = i;
            closest_frequency = frequency_i;
        }
        if (response_i > highest_response) {
            highest_response_index = i;
            highest_response = response_i;
        }
    }
    EXPECT_EQ(closest_frequency_index, highest_response_index);

    dut.cache_diag();
}

TEST(SlapFFTTest, OneFrequencyWithRateReduction) {
    FFTConfiguration config(
        44100,
        32767,
        5,
        3,
        false
    );
    float test_frequency = 2100.0;
    float test_period = config.sample_rate() / test_frequency;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(32767 * std::sinf(TAU * i / test_period));
    }

    FFT dut(
        src,
        config,
        10
    );

    Signal dst;
    dut.fft(dst, src.size() / 2);

    std::stringstream ss_fft;
    ss_fft << std::fixed << std::setprecision(3);
    for (int i = 0; i < dst.size(); ++i) {
        ss_fft << "\t" << i << " " << dut.config().frequency()[i] << " -> " << std::abs(dst[i]) << std::endl;
    }
    std::cout << "Result of FFT: " << std::endl << ss_fft.str();

    // The maximum response should be at the nearest frequency tap.
    // Check single-endedly -> dst.size() / 2 as the inclusive limit.
    int closest_frequency_index = 0;
    float closest_frequency = 0.0f;
    int highest_response_index = 0;
    float highest_response = 0.0f;
    for (int i = 0; i <= dst.size() / 2; ++i) {
        float frequency_i = dut.config().frequency()[i];
        float response_i = std::abs(dst[i]);
        if (std::abs(frequency_i - test_frequency) < std::abs(closest_frequency - test_frequency)) {
            closest_frequency_index = i;
            closest_frequency = frequency_i;
        }
        if (response_i > highest_response) {
            highest_response_index = i;
            highest_response = response_i;
        }
    }
    EXPECT_EQ(closest_frequency_index, highest_response_index);

    dut.cache_diag();
}

TEST(SlapFFTTest, TwoFrequencyNoRateReduction) {
    FFTConfiguration config(
        44100,
        32767,
        4,
        1,
        false
    );
    size_t test_tap_a = 3;
    size_t test_tap_b = 11;
    float test_frequency_a = 4000.0;
    float test_frequency_b = 15000.0;
    float test_period_a = config.sample_rate() / test_frequency_a;
    float test_period_b = config.sample_rate() / test_frequency_b;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(16383 * (std::sinf(TAU * i / test_period_a) + std::sinf(TAU * i / test_period_b)));
    }

    FFT dut(
        src,
        config,
        10
    );

    Signal dst;
    dut.fft(dst, src.size() / 2);

    std::stringstream ss_fft;
    ss_fft << std::fixed << std::setprecision(3);
    for (int i = 0; i < dst.size(); ++i) {
        ss_fft << "\t" << i << " " << dut.config().frequency()[i] << " -> " << std::abs(dst[i]) << std::endl;
    }
    std::cout << "Result of FFT: " << std::endl << ss_fft.str();

    // Check that both taps are local maxima in the FFT response.
    EXPECT_GT(std::abs(dst[test_tap_a]), std::abs(dst[test_tap_a-1]));
    EXPECT_GT(std::abs(dst[test_tap_a]), std::abs(dst[test_tap_a+1]));
    EXPECT_GT(std::abs(dst[test_tap_b]), std::abs(dst[test_tap_b-1]));
    EXPECT_GT(std::abs(dst[test_tap_b]), std::abs(dst[test_tap_b+1]));

    dut.cache_diag();
}

TEST(SlapFFTTest, CacheHit) {
    FFTConfiguration config(
        44100,
        32767,
        4,
        1,
        false
    );
    float test_frequency = 5500.0;
    float test_period = config.sample_rate() / test_frequency;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(
            32767.0f * 
            std::powf(2.0f, i / -50.0f) * 
            std::sinf(TAU * i / test_period)
        );
    }

    FFT dut(
        src,
        config,
        3
    );

    Signal dst;
    for (int i = 0; i < dut.cache_size_max(); ++i) {
        dut.fft(dst, src.size() / 2);
    }
    dut.cache_diag();

    EXPECT_EQ(dut.cache_size(), 1);
}

TEST(SlapFFTTest, CacheMissFill) {
    FFTConfiguration config(
        44100,
        32767,
        4,
        1,
        false
    );
    float test_frequency = 5500.0;
    float test_period = config.sample_rate() / test_frequency;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(
            32767.0f * 
            std::powf(2.0f, i / -50.0f) * 
            std::sinf(TAU * i / test_period)
        );
    }

    FFT dut(
        src,
        config,
        3
    );

    Signal dst;
    for (int i = 0; i < dut.cache_size_max(); ++i) {
        dut.fft(dst, i * 10);
    }
    dut.cache_diag();

    EXPECT_EQ(dut.cache_size(), dut.cache_size_max());
}

TEST(SlapFFTTest, CacheMissOverfill) {
    FFTConfiguration config(
        44100,
        32767,
        4,
        1,
        false
    );
    float test_frequency = 5500.0;
    float test_period = config.sample_rate() / test_frequency;

    Signal src;
    for (int i = 0; i < 300; ++i) {
        src.push_back(
            32767.0f * 
            std::powf(2.0f, i / -50.0f) * 
            std::sinf(TAU * i / test_period)
        );
    }

    FFT dut(
        src,
        config,
        3
    );

    Signal dst;
    for (int i = 0; i < dut.cache_size_max() * 3; ++i) {
        dut.fft(dst, i * 10);
    }
    dut.cache_diag();

    EXPECT_EQ(dut.cache_size(), dut.cache_size_max());
}

} // anonymous namespace
} // namespace Slap


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}