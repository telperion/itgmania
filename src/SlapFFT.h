#pragma once

#include <cmath>
#include <complex>
#include <list>
#include <utility>
#include <vector>
#include <unordered_map>

namespace Slap {
    using CC = std::complex<float>;
    using Window = std::vector<float>;
    using FrequencyAxis = std::vector<float>;
    using Signal = std::vector<CC>;
    using SignalCache = std::list<std::pair<size_t, const Signal>>;
    using SignalCacheLookup = std::unordered_map<size_t, SignalCache::const_iterator>;

    const double PI = 4.0f * std::atanf(1.0f);
    const double TAU = 2.0f * PI;

    class FFTConfiguration {
        public:
            /**
            @brief Configuration parameters for an FFT.

            @param sample_rate Samples per second (Hz) of the source signal.
            @param max_src_signal Maximum value of the source signal.
            @param size_p2 One-sided length of the filter window, expressed as a power on 2.
            @param rate_reduction Reduce effective sample rate by spacing out samples pulled from the input signal by this multiplier.
            @param invert IFFT, instead of FFT.
            */
            FFTConfiguration(
                float sample_rate,
                float max_src_signal,
                size_t size_p2 = 5,
                size_t rate_reduction = 1,
                bool invert = false
            );


            /**
            @brief Calculate the frequency axis for an FFT calculated with this
            configuration.

            @param index The index into the FFT.
            @return The frequency tap at the given index (Hz).
            */
            const FrequencyAxis& frequency() const {return frequency_;}


            /**
            @brief Get the length of the filter window.

            @return The length of the filter window.
            */
            size_t length() const;


            /**
            @brief Convert a specified time in seconds to source sample index.

            @return The index of the closest sample to the specified time in seconds.
            */
            size_t index(double time) const;


            /**
            @brief Convert a specified source sample index to time in seconds.

            @return Time in seconds at the specified source sample index.
            */
            double time(size_t index) const;


            const Window& window() const {return window_;}
            size_t size_p2() const {return size_p2_;}
            float sample_rate() const {return sample_rate_;}
            float max_src_signal() const {return max_src_signal_;}
            size_t rate_reduction() const {return rate_reduction_;}
            bool invert() const {return invert_;}

        protected:
            Window window_;             /**< Vector storage for the filter window */
            FrequencyAxis frequency_;   /**< Vector storage for the frequency axis of the FFT result */

            size_t size_p2_;            /**< One-sided length of the filter window as a power on 2 */
            float sample_rate_;         /**< Samples per second (Hz) of the source signal */
            float max_src_signal_;      /**< Maximum value of the source signal. */
            size_t rate_reduction_;     /**< Spacing introduced between samples used in the FFT */
            bool invert_;               /**< IFFT, instead of FFT */
    };

    class FFT {
        public:
            /**
            @brief Set up a memoized FFT.
            This memoized FFT (or IFFT, depending on configuration) is meant to
            run many times at different points in the same source signal.

            @param src Input signal.
            @param config FFT configuration parameters.
            @param cache_size_p2 Size of memoization cache, as a power on 2.
            */
            FFT(
                const Signal& src,
                const FFTConfiguration& config,
                size_t cache_size_p2 = 10
            );

            /**
            @brief Perform an FFT centered at the given index into the input signal.
        
            @param dst Storage for the result of the transform. Also placed in the cache.
            @param center_index The index into the input signal that represents the center of the window currently being analyzed.
            */
            void fft(
                Signal& dst,
                size_t center_index
            );

            /**
            @brief Get the maximum cache size.
            @return The maximum cache size.
            */
            size_t cache_size_max() const;

            /**
            @brief Get the current cache size.
            @return The current cache size.
            */
            size_t cache_size() const;

            /**
            @brief Cache diagnosis.
            */
            void cache_diag() const;

            const Signal& src() const {return src_;}
            const FFTConfiguration& config() const {return config_;}
            size_t cache_size_p2() const {return cache_size_p2_;}

        protected:
            /**
            @brief Perform an FFT centered at the given index into the input signal.
            This function does not interact with the cache; the public function FFT::fft() will perform the memoization.
        
            @param dst Storage for the result of the transform.
            @param center_index The index into the input signal that represents the center of the window currently being analyzed.
            */
            void fft_internal(
                Signal& dst,
                size_t center_index
            );

            FFTConfiguration config_;           /**< Configuration parameters */
            Signal src_;                        /**< Input signal */
            size_t cache_size_p2_;              /**< Size of memoization cache, as a power on 2 */

            SignalCache cache_;                 /**< LRU cache for previous FFT results */
            SignalCacheLookup cache_lookup_;    /**< Previous FFT result retrieval */
    };
}
