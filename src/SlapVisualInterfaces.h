#pragma once

#include "RageTexture.h"
#include "RageSurface.h"

#include "SlapFFT.h"

namespace Slap {
    class SlapSpectrogramTexture: public RageTexture {
        public:
            SlapSpectrogramTexture(RageTextureID textureID, RageSurface *spectrogram);
            ~SlapSpectrogramTexture();
            void Create();
            void Destroy();
            void SetSpectrogram(RageSurface* spectrogram);
            uintptr_t GetTexHandle() const { return m_uTexHandle; };
        protected:
            RageSurface* m_spectrogram;
            uintptr_t m_uTexHandle;
    };

    class SlapSpectrogramCache {
        public:
            SlapSpectrogramCache();
            bool LoadMusic(RString filename);
            void Update(double start, double end, double step);
            RageTexture* GetTexture() const;

        protected:
            RString m_filename;
            Signal m_signal;
            FFTConfiguration m_config;
            FFT m_fft;
            SlapSpectrogramTexture* m_texture;
    };
}
