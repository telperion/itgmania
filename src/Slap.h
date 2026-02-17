#pragma once

#include <memory>
#include <vector>
#include <iostream>
#include <functional>

#include "RageSurface.h"

#include "SlapFFT.h"

namespace Slap {
    constexpr size_t _MAX_IMAGE_DATA_SIZE = 1000000;
    constexpr size_t _EPS = 1E-6;
    using FrequencyAxis = std::vector<float>;
    using LocalResponse = std::vector<float>;

    uint32_t heatmap(float v);

    /**
    @brief Preload and decode entire music file into memory.

    TODO: replace this (and the FFT signal storage method) with a stream-friendly interface.
    */
    void preload_for_fft(
        Signal &result,
        RString filename,
        RString &error
    );

    /**
    @brief Create a RageSurface storing a spectrogram image.
    
    @param start The starting time in the source signal, inclusive.
    @param end The end time in the source signal, exclusive.
    @param step The step size for iterating through the source signal.
    @param colormap Function that converts float values from [0, 1] to colors in RGBA8 format.
    @param scale If true, scale to maximum value over window analyzed. If false (default), scale to maximum value of input signal format.
    @return A shared pointer to a new RageSurface in RGBA8 format.
    */
    std::shared_ptr<RageSurface> spectrogram(
        const FFT& fft,
        double start,
        double end,
        double step,
        std::function<uint32_t(const float&)> colormap = heatmap,
        bool scale = false
    );
}
