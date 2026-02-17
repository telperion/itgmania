#include "global.h"
#include "SlapFFT.h"

#include <cmath>
#include <complex>
#include <list>
#include <utility>
#include <vector>
#include <unordered_map>
#include <iostream>


namespace Slap {
    FFTConfiguration::FFTConfiguration(
        float sample_rate,
        float max_src_signal,
        size_t size_p2,
        size_t rate_reduction,
        bool invert
    ) : sample_rate_(sample_rate),
        max_src_signal_(max_src_signal),
        size_p2_(size_p2),
        rate_reduction_(rate_reduction),
        invert_(invert) {
        // Generate the Hann window corresponding to this configuration.
        size_t length = (2 << size_p2_);
        window_.reserve(length + 1);
        for (size_t i = 0; i <= length; ++i) {
            window_.push_back((1.0f - std::cosf((TAU * i) / length)) * 0.5f);
        }

        // Generate the frequency axis corresponding to this configuration.
        float delta_f = sample_rate_ / (rate_reduction_ * length);
        frequency_.reserve(length + 1);
        for (size_t i = 0; i <= length; ++i) {
            if (i <= length / 2) {
                frequency_.push_back(i * delta_f);
            }
            else {
                frequency_.push_back((length - i) * delta_f);
            }
        }
    }

    size_t FFTConfiguration::length() const {
        return (2 << size_p2_) + 1;
    }

    size_t FFTConfiguration::index(double time) const {
        double index = time * sample_rate_ + 0.5;
        if (index < 0) {
            return 0;
        }
        // else if (index > src_.size()) {
        //     return src_.size();
        // }
        return size_t(index);
    }

    double FFTConfiguration::time(size_t index) const {
        return double(index) / sample_rate_;
    }


    FFT::FFT(
        const Signal& src,
        const FFTConfiguration& config,
        size_t cache_size_p2
    ) : src_(src),
        config_(config),
        cache_size_p2_(cache_size_p2) {}

    template<typename T>
    const T& at_or(const std::vector<T> &v, size_t i, const T& d) {
        if (i < 0) {
            return d;
        }
        if (i >= v.size()) {
            return d;
        }
        return v[i];
    }

    size_t bit_reversal(size_t a, size_t bits) {
        size_t b = 0;
        for (size_t i = 0; i < bits; i++) {
            b <<= 1;
            b += (a & 1);
            a >>= 1;
        }
        return b;
    }

    void FFT::fft_internal(
        Signal &dst,
        size_t center_index
    ) {
        // How many steps of butterfly transform to perform?
        size_t dst_window_length = 2 << config_.size_p2();
        size_t src_window_length = dst_window_length * config_.rate_reduction();
        size_t window_offset = center_index - src_window_length / 2;

        // Prepare the elements in the window for the transform.
        dst.clear();
        dst.reserve(dst_window_length + 1);
        for (size_t i = 0; i < dst_window_length; ++i) {
            size_t bit_reversed = bit_reversal(i, config_.size_p2() + 1);
            dst.push_back(at_or(
                src_, 
                bit_reversed * config_.rate_reduction() + window_offset, 
                CC(0.0f)
            ) * config_.window()[bit_reversed]);
        }
        dst.push_back(at_or(
            src_,
            src_window_length + window_offset,
            CC(0.0f)
        ) * config_.window()[dst_window_length]);

        // Set up the butterfly transform using stride lengths.
        for (size_t stride = 2; stride <= dst_window_length; stride <<= 1) {
            size_t half_stride = stride / 2;
            float theta = (TAU / stride) * (config_.invert() ? -1 : 1);
            CC unity(std::cosf(theta), std::sinf(theta));
            for (size_t i = 0; i < dst_window_length; i += stride) {
                CC winding(1);
                for (size_t j = 0; j < half_stride; ++j) {
                    size_t butter_index = i + j;
                    size_t fly_index = i + j + half_stride;
                    CC butter(dst[butter_index]);
                    CC fly(dst[fly_index] * winding);
                    dst[butter_index] = butter + fly;
                    dst[fly_index] = butter - fly;
                    winding *= unity;
                }
            }
        }

        // Scaling when applying the IFFT.
        if (config_.invert()) {
            for (CC &x : dst) {
                x /= dst_window_length;
            }
        }
    }

    void FFT::fft(
        Signal &dst,
        size_t center_index
    ) {
        // Already in the cache?
        auto search = cache_lookup_.find(center_index);
        if (search != cache_lookup_.end()) {
            // Use the cache lookup to retrieve the stored FFT result.
            auto cache_entry = search->second;

            // Make a deep copy for external consumption.
            dst.clear();
            dst.assign(cache_entry->second.begin(), cache_entry->second.end());

            // Reinsert the FFT result at the front of the cache.
            cache_.emplace_front(center_index, dst);
            cache_.erase(cache_entry);
        }
        else {
            // Calculate the fresh FFT.
            fft_internal(dst, center_index);

            // Kick something out of the cache if necessary.
            if (cache_size() >= cache_size_max()) {
                auto cache_oldest = cache_.back();
                cache_lookup_.erase(cache_oldest.first);
                cache_.pop_back();
            }

            // Insert the FFT result at the front of the cache.
            cache_.emplace_front(center_index, dst);
        }

        // Update the cache lookup.
        cache_lookup_[center_index] = cache_.cbegin();
    }

    size_t FFT::cache_size_max() const {
        return (1 << cache_size_p2_);
    }

    size_t FFT::cache_size() const {
        return cache_.size();
    }

    void FFT::cache_diag() const {
        std::cout << "Cache:" << std::endl;
        for (const auto& it : cache_) {
            std::cout << "\t" << it.first << ": ";
            for (const auto& v : it.second) {
                std::cout << v << ", ";
            }
            std::cout << std::endl;
        }
        std::cout << "Cache Lookup:" << std::endl;
        for (const auto& it : cache_lookup_) {
            std::cout << "\t" << it.first << ": (" << it.second->first << " -> ";
            for (const auto& v : it.second->second) {
                std::cout << v << ", ";
            }
            std::cout << ")" << std::endl;
        }
    }
}
