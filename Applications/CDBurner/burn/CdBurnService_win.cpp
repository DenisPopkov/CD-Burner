#include "CdBurnPlatform.h"
#include "../../../Source/export/RedBookExporter.h"
#include <juce_audio_formats/juce_audio_formats.h>

#if JUCE_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <imapi2.h>
#include <imapi2fs.h>
#include <cmath>
#include <cstring>
#include <vector>

namespace cassette::cdburn_platform
{

namespace
{

void reportProgress(const CdBurnService::ProgressCallback& onProgress,
                    int trackIndex,
                    int trackCount,
                    double percent,
                    const juce::String& message)
{
    if (!onProgress)
        return;

    CdBurnProgress progress;
    progress.trackIndex = trackIndex;
    progress.trackCount = trackCount;
    progress.overallPercent = percent;
    progress.message = message;
    onProgress(progress);
}

juce::String hresultMessage(HRESULT hr)
{
    switch (static_cast<ULONG>(hr))
    {
        case 0xC0AA050D: return "audio stream rejected by Windows (invalid IStream / format)";
        case 0xC0AA0509: return "not enough space on the disc for this track";
        case 0xC0AA0508: return "too many tracks (CD limit is 99)";
        case 0xC0AA0502: return "media was not prepared";
        case 0xC0AA0504: return "only blank CD-R/CD-RW is supported";
        case 0xC0AA050F: return "invalid client name";
        case 0xC0AA0202: return "no compatible recorder / media";
        default: break;
    }

    return "Windows burn error 0x" + juce::String::toHexString(static_cast<int>(hr));
}

/** Raw Red Book PCM (44.1 kHz / 16-bit / stereo), padded to 2352-byte sectors. */
std::vector<uint8_t> loadRedBookPcmSectors(const juce::File& wavFile)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(wavFile));
    if (reader == nullptr)
        return {};

    if (reader->numChannels != 2
        || std::abs(reader->sampleRate - kRedBookSampleRate) > 1.0)
        return {};

    const auto numSamples = static_cast<int>(reader->lengthInSamples);
    if (numSamples <= 0)
        return {};

    juce::AudioBuffer<float> floatBuffer(2, numSamples);
    if (! reader->read(&floatBuffer, 0, numSamples, 0, true, true))
        return {};

    std::vector<uint8_t> bytes(static_cast<size_t>(numSamples) * 4u);
    auto* dst = reinterpret_cast<int16_t*>(bytes.data());
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float sample = floatBuffer.getSample(ch, i);
            dst[i * 2 + ch] = static_cast<int16_t>(
                juce::jlimit(-32768, 32767, static_cast<int>(std::lround(sample * 32767.0f))));
        }
    }

    // IMAPI requires the audio stream length to be a multiple of a CDDA sector (2352 bytes).
    constexpr size_t sectorSize = 2352;
    // Minimum audio track length is 4 seconds (~705600 bytes). Pad if shorter.
    constexpr size_t minTrackBytes = 44100u * 2u * 2u * 4u;
    if (bytes.size() < minTrackBytes)
        bytes.resize(minTrackBytes, 0);

    const size_t remainder = bytes.size() % sectorSize;
    if (remainder != 0)
        bytes.resize(bytes.size() + (sectorSize - remainder), 0);

    return bytes;
}

HRESULT createPcmStream(const std::vector<uint8_t>& pcm, IStream** outStream)
{
    if (outStream == nullptr)
        return E_POINTER;
    *outStream = nullptr;

    if (pcm.empty())
        return E_INVALIDARG;

    // CreateStreamOnHGlobal provides a complete IStream (Seek/Stat/Read) that IMAPI accepts.
    // Returning STG_E_INVALIDFUNCTION from Seek/Stat causes E_IMAPI_DF2TAO_STREAM_NOT_SUPPORTED.
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, pcm.size());
    if (memory == nullptr)
        return E_OUTOFMEMORY;

    void* locked = ::GlobalLock(memory);
    if (locked == nullptr)
    {
        ::GlobalFree(memory);
        return E_OUTOFMEMORY;
    }

    std::memcpy(locked, pcm.data(), pcm.size());
    ::GlobalUnlock(memory);

    IStream* stream = nullptr;
    HRESULT hr = ::CreateStreamOnHGlobal(memory, TRUE, &stream);
    if (FAILED(hr))
    {
        ::GlobalFree(memory);
        return hr;
    }

    // Ensure Stat reports the exact PCM size (GlobalAlloc size can round up).
    ULARGE_INTEGER size {};
    size.QuadPart = static_cast<ULONGLONG>(pcm.size());
    hr = stream->SetSize(size);
    if (FAILED(hr))
    {
        stream->Release();
        return hr;
    }

    LARGE_INTEGER zero {};
    zero.QuadPart = 0;
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    *outStream = stream;
    return S_OK;
}

struct ComScope
{
    bool owned = false;
    explicit ComScope(HRESULT hr) : owned(hr == S_OK || hr == S_FALSE) {}
    ~ComScope()
    {
        if (owned)
            CoUninitialize();
    }
};

} // namespace

juce::String platformBurnHint()
{
    return "Insert a blank CD-R and ensure a writable optical drive is connected.";
}

std::vector<CdBurnDevice> listDevices()
{
    std::vector<CdBurnDevice> devices;

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ComScope scope(coHr);

    IDiscMaster2* discMaster = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MsftDiscMaster2,
                                  nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&discMaster));
    if (FAILED(hr))
        return devices;

    LONG count = 0;
    if (SUCCEEDED(discMaster->get_Count(&count)))
    {
        for (LONG i = 0; i < count; ++i)
        {
            BSTR id = nullptr;
            if (SUCCEEDED(discMaster->get_Item(i, &id)) && id != nullptr)
            {
                CdBurnDevice device;
                device.id = juce::String(id);
                device.displayName = device.id;
                devices.push_back(std::move(device));
                SysFreeString(id);
            }
        }
    }

    discMaster->Release();
    return devices;
}

CdBurnResult burnAudioDisc(const CdBurnDevice& device,
                           const juce::Array<juce::File>& trackFiles,
                           CdBurnService::ProgressCallback onProgress)
{
    if (trackFiles.isEmpty())
        return { false, "No prepared tracks to burn." };

    for (const auto& track : trackFiles)
    {
        if (!RedBookExporter::isRedBookWavFile(track))
            return { false, "Prepared tracks must be 16-bit / 44.1 kHz WAV: " + track.getFileName() };
    }

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ComScope scope(coHr);

    IDiscRecorder2* recorder = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MsftDiscRecorder2,
                                  nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&recorder));
    if (FAILED(hr))
        return { false, "Failed to create disc recorder (" + hresultMessage(hr) + ")" };

    BSTR uniqueId = SysAllocString(device.id.toWideCharPointer());
    hr = recorder->InitializeDiscRecorder(uniqueId);
    SysFreeString(uniqueId);
    if (FAILED(hr))
    {
        recorder->Release();
        return { false, "Failed to initialize recorder (" + hresultMessage(hr) + ")" };
    }

    IDiscFormat2TrackAtOnce* trackAtOnce = nullptr;
    hr = CoCreateInstance(CLSID_MsftDiscFormat2TrackAtOnce,
                          nullptr,
                          CLSCTX_ALL,
                          IID_PPV_ARGS(&trackAtOnce));
    if (FAILED(hr))
    {
        recorder->Release();
        return { false, "Failed to create burn engine (" + hresultMessage(hr) + ")" };
    }

    hr = trackAtOnce->put_Recorder(recorder);
    if (FAILED(hr))
    {
        trackAtOnce->Release();
        recorder->Release();
        return { false, "Failed to select recorder (" + hresultMessage(hr) + ")" };
    }

    BSTR clientName = SysAllocString(L"CD Burner");
    hr = trackAtOnce->put_ClientName(clientName);
    SysFreeString(clientName);
    if (FAILED(hr))
    {
        trackAtOnce->Release();
        recorder->Release();
        return { false, "Failed to set burn client (" + hresultMessage(hr) + ")" };
    }

    VARIANT_BOOL supported = VARIANT_FALSE;
    if (SUCCEEDED(trackAtOnce->IsRecorderSupported(recorder, &supported)) && supported == VARIANT_FALSE)
    {
        trackAtOnce->Release();
        recorder->Release();
        return { false, "This drive does not support audio CD burning." };
    }

    hr = trackAtOnce->PrepareMedia();
    if (FAILED(hr))
    {
        trackAtOnce->Release();
        recorder->Release();
        return { false, "Insert a blank CD-R/CD-RW (" + hresultMessage(hr) + ")" };
    }

    const int trackCount = trackFiles.size();
    for (int i = 0; i < trackCount; ++i)
    {
        reportProgress(onProgress,
                       i + 1,
                       trackCount,
                       static_cast<double>(i) / static_cast<double>(trackCount),
                       "Burning track " + juce::String(i + 1) + "/" + juce::String(trackCount));

        const auto pcm = loadRedBookPcmSectors(trackFiles[i]);
        if (pcm.empty())
        {
            trackAtOnce->ReleaseMedia();
            trackAtOnce->Release();
            recorder->Release();
            return { false, "Failed to read track: " + trackFiles[i].getFileName() };
        }

        IStream* stream = nullptr;
        hr = createPcmStream(pcm, &stream);
        if (FAILED(hr) || stream == nullptr)
        {
            trackAtOnce->ReleaseMedia();
            trackAtOnce->Release();
            recorder->Release();
            return { false, "Failed to create audio stream (" + hresultMessage(hr) + ")" };
        }

        hr = trackAtOnce->AddAudioTrack(stream);
        stream->Release();
        if (FAILED(hr))
        {
            trackAtOnce->ReleaseMedia();
            trackAtOnce->Release();
            recorder->Release();
            return { false,
                     "Failed to write track " + juce::String(i + 1) + " (" + hresultMessage(hr) + ")" };
        }
    }

    trackAtOnce->ReleaseMedia();
    trackAtOnce->Release();
    recorder->EjectMedia();
    recorder->Release();

    reportProgress(onProgress, trackCount, trackCount, 1.0, "Burn complete");
    return { true, {} };
}

}

#endif
