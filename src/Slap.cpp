#include "global.h"
#include "Slap.h"

#include "RageLog.h"
#include "RageSound.h"
#include "RageSoundReader_FileReader.h"

#include <ctime>
#include <memory>



namespace Slap {
    uint32_t heatmap(float v) {
        // ABGR order
        if (v < 0) {
            return 0xFF000000;
        }
        if (v < 0.1) {
            return 0xFF000000 + int(0xFF * v / 0.1);
        }
        if (v < 0.4) {
            return 0xFF0000FF + (int(0xFF * (v - 0.1) / 0.3) << 8);
        }
        if (v < 1.0) {
            return 0xFF00FFFF + (int(0xFF * (v - 0.4) / 0.6) << 16);
        }
        return 0xFFFFFFFF;
    }


    void preload_for_fft(
        AudioResult &result,
        RString filename,
        RString &error
    ) {
        auto reader = RageSoundReader_FileReader::OpenFile(
            filename,
            error,
            nullptr
        );
        if (reader == nullptr || !error.empty()) {
            error = "Failed to open audio file for spectrogram: " + error;
            return;
        }
        result.sampleRate = reader->GetSampleRate();
        result.nChannels = reader->GetNumChannels();
        result.signal.clear();

        if (result.nChannels < 1 or result.nChannels > 2) {
            LOG->Warn("Audio file %s says it has %i channels; interpreting as stereo", filename.c_str(), result.nChannels);
            result.nChannels = 2;
        }

        LOG->Info("Preloading audio file %s with sample rate %i and %i channels", filename.c_str(), result.sampleRate, result.nChannels);
        std::vector<float> buffer;
        int frameSize = 1024;
        int chunkSize = frameSize * result.nChannels;
        int framesRead = 0;
        int nextFrame = 0;
        buffer.resize(chunkSize, 0.0f);
        while (nextFrame < 240 * result.sampleRate) {
            framesRead = reader->RetriedRead(buffer.data(), frameSize, &nextFrame);
            if (framesRead <= 0) {
                break;
            }
            for (int i = 0; i < framesRead; ++i) {
                float mixdown = 0.0f;
                for (int c = 0; c < result.nChannels; ++c) {
                    mixdown += buffer[i * result.nChannels + c];
                }
                result.signal.push_back(mixdown / result.nChannels);
            }
        };
    }

    
    RageSurface* spectrogram(
        FFT& fft,
        double start,
        double end,
        double step,
        std::function<uint32_t(const float&)> colormap,
        bool scale,
        bool flip_axes
    ) {
        // Width is set by the FFT, taken single-ended.
        size_t frequency_axis = fft.config().length() / 2 + 1;
        // Length depends on the range parameters.
        size_t time_axis = size_t((end - start) / step);

        if (frequency_axis * time_axis > _MAX_IMAGE_DATA_SIZE) {
            LOG->Warn("Image data size (%zu frequency taps * %zu measurements) exceeds maximum allowed: %zu", frequency_axis, time_axis, _MAX_IMAGE_DATA_SIZE);
            return nullptr;
        }

        // Create surface and reserve memory.
        size_t width = flip_axes ? time_axis : frequency_axis;
        size_t height = flip_axes ? frequency_axis : time_axis;
        RageSurface* image = CreateSurface(
            width,
            height,
            32,             // 4 bytes per pixel
            0x000000FF,     // Mask for red
            0x0000FF00,     // Mask for green
            0x00FF0000,     // Mask for blue
            0xFF000000      // Mask for alpha
        );

        // Fill the image with FFT data.
        Signal fft_data;

        // Find the visual scaling factor.
        float fft_data_max = fft.config().max_src_signal();
        if (scale) {
            fft_data_max = 0.0f;
            for (size_t row = 0; row < time_axis; ++row) {
                size_t center_index = fft.config().index(start + step * row);
                fft.fft(fft_data, center_index);
                for (size_t f_index = 0; f_index < frequency_axis; ++f_index) {
                    float f = std::abs(fft_data[f_index]);
                    fft_data_max = (fft_data_max > f) ? fft_data_max : f;
                }
            }
            if (fft_data_max < _EPS) {
                // Signal is too low.
                fft_data_max = 1.0f;
            }
        }

        // Map values to colors and transfer to the pixel storage.
        for (size_t row = 0; row < time_axis; ++row) {
            size_t center_index = fft.config().index(start + step * row);
            fft.fft(fft_data, center_index);
            for (size_t f_index = 0; f_index < frequency_axis; ++f_index) {
                uint32_t color = colormap(std::abs(fft_data[f_index]) / fft_data_max);
                auto major_axis_index = flip_axes ? f_index : row;
                auto minor_axis_index = flip_axes ? row : f_index;
                *((uint32_t *)(image->pixels + major_axis_index*image->pitch + minor_axis_index*4)) = color;
            }
        }

        LOG->Info("Spectrogram info: w=%i, h=%i, bpp=%i", image->w, image->h, image->fmt.BitsPerPixel);
        return image;
    }
}
