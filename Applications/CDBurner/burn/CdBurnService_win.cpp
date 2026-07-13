#include "CdBurnPlatform.h"
#include "../../../Source/export/RedBookExporter.h"

#if JUCE_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <imapi2.h>
#include <imapi2fs.h>
#include <atomic>
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
    return "Windows burn error 0x" + juce::String::toHexString(static_cast<int>(hr));
}

std::vector<uint8_t> loadRedBookPcmSectors(const juce::File& wavFile)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wavFile));
    if (reader == nullptr)
        return {};

    if (reader->numChannels != 2 || reader->bitsPerSample != 16
        || std::abs(reader->sampleRate - kRedBookSampleRate) > 1.0)
        return {};

    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),
                                      static_cast<int>(reader->lengthInSamples));
    reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    std::vector<int16_t> interleaved(static_cast<size_t>(buffer.getNumSamples()) * 2);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float sample = buffer.getSample(ch, i);
            interleaved[static_cast<size_t>(i) * 2 + static_cast<size_t>(ch)] =
                static_cast<int16_t>(juce::jlimit(-32768, 32767, static_cast<int>(std::lround(sample * 32767.0f))));
        }
    }

    std::vector<uint8_t> bytes(interleaved.size() * sizeof(int16_t));
    std::memcpy(bytes.data(), interleaved.data(), bytes.size());

    constexpr size_t sectorSize = 2352;
    const size_t remainder = bytes.size() % sectorSize;
    if (remainder != 0)
        bytes.resize(bytes.size() + (sectorSize - remainder), 0);

    return bytes;
}

class PcmIStream final : public IStream
{
public:
    static HRESULT create(const std::vector<uint8_t>& dataIn, IStream** outStream)
    {
        if (outStream == nullptr)
            return E_POINTER;

        auto* stream = new PcmIStream(dataIn);
        *outStream = stream;
        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (ppvObject == nullptr)
            return E_POINTER;

        if (riid == IID_IUnknown || riid == IID_IStream)
        {
            *ppvObject = static_cast<IStream*>(this);
            AddRef();
            return S_OK;
        }

        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(++refCount); }

    STDMETHODIMP_(ULONG) Release() override
    {
        const auto count = --refCount;
        if (count == 0)
            delete this;
        return static_cast<ULONG>(count);
    }

    STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead) override
    {
        if (pv == nullptr)
            return STG_E_INVALIDPOINTER;

        const auto available = data.size() - position;
        const auto toRead = static_cast<size_t>(juce::jmin<ULONG>(cb, static_cast<ULONG>(available)));
        if (toRead > 0)
            std::memcpy(pv, data.data() + position, toRead);
        position += toRead;
        if (pcbRead != nullptr)
            *pcbRead = static_cast<ULONG>(toRead);
        return toRead == cb ? S_OK : S_FALSE;
    }

    STDMETHODIMP Write(const void*, ULONG, ULONG*) override { return STG_E_ACCESSDENIED; }
    STDMETHODIMP Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER*) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP SetSize(ULARGE_INTEGER) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP Commit(DWORD) override { return S_OK; }
    STDMETHODIMP Revert() override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP Stat(STATSTG*, DWORD) override { return STG_E_INVALIDFUNCTION; }
    STDMETHODIMP Clone(IStream**) override { return STG_E_INVALIDFUNCTION; }

private:
    explicit PcmIStream(const std::vector<uint8_t>& dataIn)
        : data(dataIn)
    {
    }

    std::vector<uint8_t> data;
    size_t position = 0;
    std::atomic<ULONG> refCount { 1 };
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
    const bool coOwned = SUCCEEDED(coHr);

    IDiscMaster2* discMaster = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MsftDiscMaster2,
                                  nullptr,
                                  CLSCTX_LOCAL_SERVER,
                                  IID_PPV_ARGS(&discMaster));
    if (FAILED(hr))
    {
        if (coOwned)
            CoUninitialize();
        return devices;
    }

    IEnumVARIANT* enumerator = nullptr;
    hr = discMaster->EnumDiscRecorders(&enumerator);
    if (SUCCEEDED(hr) && enumerator != nullptr)
    {
        VARIANT variant;
        VariantInit(&variant);
        ULONG fetched = 0;
        while (enumerator->Next(1, &variant, &fetched) == S_OK)
        {
            if (variant.vt == VT_BSTR && variant.bstrVal != nullptr)
            {
                CdBurnDevice device;
                device.id = juce::String(variant.bstrVal);
                device.displayName = device.id;
                devices.push_back(std::move(device));
            }
            VariantClear(&variant);
        }
        enumerator->Release();
    }

    discMaster->Release();
    if (coOwned)
        CoUninitialize();

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
    const bool coOwned = SUCCEEDED(coHr);

    IDiscRecorder2* recorder = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MsftDiscRecorder2,
                                  nullptr,
                                  CLSCTX_LOCAL_SERVER,
                                  IID_PPV_ARGS(&recorder));
    if (FAILED(hr))
    {
        if (coOwned)
            CoUninitialize();
        return { false, "Failed to create disc recorder (" + hresultMessage(hr) + ")" };
    }

    const auto idWide = device.id.toWideCharPointer();
    BSTR uniqueId = SysAllocString(idWide);
    hr = recorder->InitializeDiscRecorder(uniqueId);
    SysFreeString(uniqueId);
    if (FAILED(hr))
    {
        recorder->Release();
        if (coOwned)
            CoUninitialize();
        return { false, "Failed to initialize recorder (" + hresultMessage(hr) + ")" };
    }

    IDiscFormat2TrackAtOnce* trackAtOnce = nullptr;
    hr = CoCreateInstance(CLSID_MsftDiscFormat2TrackAtOnce,
                          nullptr,
                          CLSCTX_LOCAL_SERVER,
                          IID_PPV_ARGS(&trackAtOnce));
    if (FAILED(hr))
    {
        recorder->Release();
        if (coOwned)
            CoUninitialize();
        return { false, "Failed to create burn engine (" + hresultMessage(hr) + ")" };
    }

    trackAtOnce->put_Recorder(recorder);
    trackAtOnce->put_ClientName(SysAllocString(L"CD Burner"));

    hr = trackAtOnce->PrepareMedia();
    if (FAILED(hr))
    {
        trackAtOnce->Release();
        recorder->Release();
        if (coOwned)
            CoUninitialize();
        return { false, "Insert a blank CD-R (" + hresultMessage(hr) + ")" };
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
            if (coOwned)
                CoUninitialize();
            return { false, "Failed to read track: " + trackFiles[i].getFileName() };
        }

        IStream* stream = nullptr;
        hr = PcmIStream::create(pcm, &stream);
        if (FAILED(hr))
        {
            trackAtOnce->ReleaseMedia();
            trackAtOnce->Release();
            recorder->Release();
            if (coOwned)
                CoUninitialize();
            return { false, "Failed to create audio stream (" + hresultMessage(hr) + ")" };
        }

        hr = trackAtOnce->AddAudioTrack(stream);
        stream->Release();
        if (FAILED(hr))
        {
            trackAtOnce->ReleaseMedia();
            trackAtOnce->Release();
            recorder->Release();
            if (coOwned)
                CoUninitialize();
            return { false, "Failed to write track " + juce::String(i + 1) + " (" + hresultMessage(hr) + ")" };
        }
    }

    trackAtOnce->ReleaseMedia();
    trackAtOnce->Release();
    recorder->Release();
    if (coOwned)
        CoUninitialize();

    reportProgress(onProgress, trackCount, trackCount, 1.0, "Burn complete");
    return { true, {} };
}

}

#endif
