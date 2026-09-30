#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

class NorthstarMasteringAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int eqBandCount = 15;
    static constexpr int stereoBandCount = 4;

    NorthstarMasteringAudioProcessor();
    ~NorthstarMasteringAudioProcessor() override = default;

    using juce::AudioProcessor::processBlock;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void requestAnalysis();
    bool isAnalysisRunning() const noexcept { return analysisRunning.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    float getLoudnessEstimate() const noexcept { return loudnessEstimate.load(); }
    float getPeakDb() const noexcept { return peakDb.load(); }
    float getOutputLoudness() const noexcept { return outputLoudness.load(); }
    float getGainReduction() const noexcept { return gainReductionDb.load(); }
    bool hasAnalysis() const noexcept { return analysisComplete.load(); }
    void copySpectrum(std::array<float, 64>& destination) const noexcept;

    float getEQFrequency(int band) const noexcept;
    float getEQGain(int band) const noexcept;
    float getEQQ(int band) const noexcept;
    bool getEQDynamic(int band) const noexcept;
    void setEQFrequency(int band, float value);
    void setEQGain(int band, float value);
    void setEQQ(int band, float value);
    void setEQDynamic(int band, bool enabled);
    void resetEQToAuto();

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    static constexpr int spectrumSize = 2048;
    static constexpr int spectrumBinCount = 64;
    static constexpr int analysisDurationSeconds = 10;
    static constexpr int linearPhaseTaps = 513;

    void updateEQFilters();
    void updateLinearPhaseFilters();
    void finishAnalysis();
    void updateCompressorCoefficients();
    void updateCrossovers();
    void applyStereoImage(float& left, float& right);
    float applySaturation(float sample, float mix, int preset) const noexcept;
    static float toDb(float value) noexcept;
    static juce::String eqId(const char* prefix, int band);
    static juce::String stereoId(const char* prefix, int band);

    double currentSampleRate = 44100.0;
    int maximumBlockSize = 1024;

    std::array<std::array<Filter, 2>, eqBandCount> eqFilters;
    std::array<juce::dsp::FIR::Filter<float>, 2> linearEqFilters;
    juce::dsp::FIR::Coefficients<float>::Ptr linearEqCoefficients;
    juce::dsp::FFT linearPhaseFft { 10 };
    std::array<float, 2048> linearPhaseWork {};
    float lastLinearPhaseSignature = -100000.0f;
    std::array<std::array<Filter, 2>, 3> loudnessFilters;
    std::array<std::array<Filter, 2>, 3> crossoverFilters;
    std::array<float, stereoBandCount - 1> crossoverFrequencies { 120.0f, 1000.0f, 6000.0f };
    std::array<std::array<float, stereoBandCount>, 2> stereoLowStates {};

    std::array<std::atomic<float>, spectrumBinCount> spectrumBins {};
    juce::dsp::FFT spectrumFft { 11 };
    juce::dsp::WindowingFunction<float> spectrumWindow {
        spectrumSize, juce::dsp::WindowingFunction<float>::hann, false
    };
    std::array<float, spectrumSize> spectrumFifo {};
    std::array<float, spectrumSize * 2> spectrumWork {};
    int spectrumFifoPosition = 0;

    std::array<float, eqBandCount> dynamicEnvelopes {};
    float compressorEnvelope = 0.0f;
    float smoothPeak = -60.0f;
    float smoothInputLoudness = -60.0f;
    float smoothOutputLoudness = -60.0f;
    float lastGainReduction = 0.0f;

    std::atomic<bool> analysisRequested { false };
    std::atomic<bool> analysisRunning { false };
    std::atomic<bool> analysisComplete { false };
    std::atomic<float> analysisProgress { 0.0f };
    std::atomic<float> loudnessEstimate { -60.0f };
    std::atomic<float> outputLoudness { -60.0f };
    std::atomic<float> peakDb { -60.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    bool collectingAnalysis = false;
    int64_t analyzedSamples = 0;
    double analysisEnergy = 0.0;
    double analysisPeak = 0.0;
    std::array<double, 3> analysisBandEnergy {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NorthstarMasteringAudioProcessor)
};