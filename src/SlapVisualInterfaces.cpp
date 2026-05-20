#include "global.h"
#include "SlapVisualInterfaces.h"

#include "RageDisplay.h"
#include "RageLog.h"
#include "RageSurfaceUtils.h"
#include "RageTextureManager.h"

#include "Slap.h"

namespace Slap {
    SlapSpectrogramTexture::SlapSpectrogramTexture(RageTextureID textureID, RageSurface *spectrogram) : RageTexture(textureID), m_spectrogram(spectrogram), m_uTexHandle(0) {
        Create();
    }

    SlapSpectrogramTexture::~SlapSpectrogramTexture() {
        Destroy();
    }

    void SlapSpectrogramTexture::Destroy() {
        DISPLAY->DeleteTexture(m_uTexHandle);
        m_uTexHandle = 0;
        if (m_spectrogram != nullptr) {
            delete m_spectrogram;
            m_spectrogram = nullptr;
        }
    }

    void SlapSpectrogramTexture::Create() {
        RageTextureID actualID = GetID();
        // actualID.Policy = RageTextureID::TEX_VOLATILE;

        // Tolerate empty spectrogram.
        if (m_spectrogram == nullptr) {
            m_spectrogram = RageSurfaceUtils::MakeDummySurface(64, 64);
            ASSERT(m_spectrogram != nullptr);
        }

        // Spectrogram defines source dimensions.
        m_iSourceWidth = m_spectrogram->w;
        m_iSourceHeight = m_spectrogram->h;

        // Texture dimensions should be powers of two.
        m_iTextureWidth = power_of_two(m_iSourceWidth);
        m_iTextureHeight = power_of_two(m_iSourceHeight);

        // Image dimensions are the same as the source.
        m_iImageWidth = m_iSourceWidth;
        m_iImageHeight = m_iSourceHeight;

        // Only one frame.
        m_iFramesWide = 1;
        m_iFramesHigh = 1;
    
        // Make sure texture dimensions aren't over max.
        static const int iMaxTextureSize = DISPLAY->GetMaxTextureSize();
        ASSERT_M( m_iTextureWidth <= iMaxTextureSize, ssprintf("w %i, %i", m_iTextureWidth, iMaxTextureSize) );
        ASSERT_M( m_iTextureHeight <= iMaxTextureSize, ssprintf("h %i, %i", m_iTextureHeight, iMaxTextureSize) );

        // Create texture from spectrogram surface data.
        RageSurfaceUtils::ConvertSurface(
            m_spectrogram,
            m_iTextureWidth,
            m_iTextureHeight,
            m_spectrogram->fmt.BitsPerPixel,
            m_spectrogram->fmt.Mask[0],
            m_spectrogram->fmt.Mask[1],
            m_spectrogram->fmt.Mask[2],
            m_spectrogram->fmt.Mask[3]
        );
        m_uTexHandle = DISPLAY->CreateTexture(RagePixelFormat_RGBA8, m_spectrogram, false);
        CreateFrameRects();
        
        if (TEXTUREMAN->IsTextureRegistered(actualID)) {
            LOG->Warn("Spectrogram texture for %s already registered", GetID().filename.c_str());
        }
        else {
            TEXTUREMAN->RegisterTexture(actualID, this);
            // TEXTUREMAN->UnloadTexture(this);
        }
    }

    void SlapSpectrogramTexture::SetSpectrogram(RageSurface* spectrogram) {
        // Update the texture by destroying the old one and creating a new one.
        Destroy();
        m_spectrogram = spectrogram;
        Create();
    }
    

    SlapSpectrogramCache::SlapSpectrogramCache() : m_filename(""), m_signal(), m_config(44100, 1.0f), m_fft(m_signal, m_config), m_texture(nullptr) {
    }

    bool SlapSpectrogramCache::LoadMusic(RString filename) {
        this->m_filename = filename;

        AudioResult result;
        RString error;
        preload_for_fft(result, filename, error);

        if (!error.empty() || result.signal.empty()) {
            LOG->Warn("Failed to preload music file %s: %s", filename.c_str(), error.c_str());
            return false;
        }

        m_filename = filename;
        m_signal = result.signal;
        m_config = FFTConfiguration(result.sampleRate, 1.0f); // TODO: how do we get max_src_signal?
        m_fft = FFT(m_signal, m_config);
        LOG->Info("Loaded music file %s with sample rate %i and %zu samples", filename.c_str(), result.sampleRate, result.signal.size());
        
        m_texture = new SlapSpectrogramTexture(RageTextureID("spectrogram_" + m_filename), nullptr);
        if (m_texture == nullptr) {
            LOG->Warn("Failed to create spectrogram texture for %s", filename.c_str());
            return false;
        }
        auto length = double(m_signal.size()) / m_config.sample_rate();
        Update(0, length, length / 64);
        LOG->Info("Constructed and registered spectrogram texture for %s", filename.c_str());

        return true;
    }

    void SlapSpectrogramCache::Update(double start, double end, double step) {
        auto spectrogram_data = spectrogram(m_fft, start, end, step, heatmap, false, true);
        if (spectrogram_data == nullptr) {
            LOG->Warn("Failed to generate spectrogram for range %f-%f with step %f", start, end, step);
            return;
        }
        LOG->Info("Updated spectrogram range %f-%f with step %f", start, end, step);
        LOG->Info("Spectrogram info: w=%i, h=%i, bpp=%i", spectrogram_data->w, spectrogram_data->h, spectrogram_data->fmt.BitsPerPixel);
        m_texture->SetSpectrogram(spectrogram_data);
    }

    RageTexture* SlapSpectrogramCache::GetTexture() const {
        return m_texture;
    }

}