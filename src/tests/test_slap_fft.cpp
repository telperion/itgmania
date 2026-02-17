#include "../SlapFFT.h"

#include <iomanip>
#include <ios>
#include <iostream>

#include "gtest/gtest.h"

namespace Slap {
namespace {

TEST(SlapFFTTest, Getters) {
    auto config = FFTConfiguration(
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

    // Length of the filter window should be 2^(size_p2).
    EXPECT_EQ(config.length(), 1 << 6);

    // Do a couple time/index conversions. These should disregard the rate reduction.
    EXPECT_EQ(config.index(10), 441000);
    EXPECT_EQ(config.time(220500), 5);
};

TEST(SlapFFTTest, CheckWindowCalculation) {
    auto config = FFTConfiguration(
        44100,
        32767,
        4,
        3,
        false
    );

    std::stringstream ss_frequency, ss_window;
    ss_frequency << std::fixed << std::setprecision(3);
    ss_window << std::fixed << std::setprecision(3);
    for (auto f : config.frequency()) {
        ss_frequency << f << " ";
    }
    for (auto w : config.window()) {
        ss_window << w << " ";
    }
    std::cout << "Frequency axis: " << ss_frequency.str();
    std::cout << "Window: " << ss_window.str();

    EXPECT_EQ(config.frequency()[1 << 4], 0);       // TODO
    EXPECT_NEAR(config.frequency()[0], 1, 1e-6);    // TODO
};

TEST(SlapFFTTest, Teapot) {
    EXPECT_EQ(7 * 6, 42);
}

} // anonymous namespace
} // namespace Slap


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}