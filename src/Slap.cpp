#include "global.h"
#include "Slap.h"

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
        Signal &result,
        RString filename,
        RString &error
    ) {
        auto reader = RageSoundReader_FileReader::OpenFile(
            filename,
            error,
            nullptr
        );

        result.clear();
        float *buffer = new float[1024];
        int chunkSize = 1024;
        int nextFrame = 0;
        int framesRead = 0;
        while (nextFrame < 240 * 48000) {
            framesRead = reader->RetriedRead(buffer, chunkSize, &nextFrame);
            if (framesRead <= 0) {
                break;
            }
            for (int i = 0; i < framesRead; ++i) {
                result.push_back(buffer[i]);
            }
        };
    }

    
    std::shared_ptr<RageSurface> spectrogram(
        FFT& fft,
        double start,
        double end,
        double step,
        std::function<uint32_t(const float&)> colormap,
        bool scale
    ) {
        // Width is set by the FFT, taken single-ended.
        size_t width = fft.config().length() / 2 + 1;
        // Length depends on the range parameters.
        size_t height = size_t((end - start) / step);

        // Create surface and reserve memory.
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
            for (size_t row = 0; row < height; ++row) {
                size_t center_index = fft.config().index(start + step * row);
                fft.fft(fft_data, row);
                for (size_t f_index = 0; f_index < width; ++f_index) {
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
        for (size_t row = 0; row < height; ++row) {
            size_t center_index = fft.config().index(start + step * row);
            fft.fft(fft_data, row);
            for (size_t f_index = 0; f_index < width; ++f_index) {
                uint32_t color = heatmap(std::abs(fft_data[f_index]) / fft_data_max);
                *((uint32_t *)(image->pixels + row*image->pitch + f_index*4)) = color;
            }
        }

        // TODO: return image;
        return std::make_shared<RageSurface>();
    }
}
