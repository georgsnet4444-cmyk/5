#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float safeFloor = 1.0e-9f;
constexpr float pi = juce::MathConstants<float>::pi;

juce::AudioParameterFloatAttributes label(const juce::String& value)
{
    return juce::AudioParameterFloatAttributes().withLabel(value);
}

juce::NormalisableRange<float> range(float start, float end, float interval = 0.01f)
{
    return juce::NormalisableRange<float>(start, end, interval);
}
}

NorthstarMasteringAudioProcessor::NorthstarMasteringAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "NORTHSTAR_STATE", createParameterLayout())
{
}

juce::String NorthstarMasteringAudioProcessor::eqId(const char* prefix, int band)
{
    return juce::String(prefix) + juce::String(band + 1);
}

juce::String NorthstarMasteringAudioProcessor::stereoId(const char* prefix, int band)
{
    return juce::String(prefix) + juce::String(band + 1);
}

juce::AudioProcessorValueTreeState::ParameterLayout
NorthstarMasteringAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<Choice>(
        juce::ParameterID { "eqMode", 1 }, "EQ mode",
        juce::StringArray { "Automatic", "Manual" }, 0));
    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "eqLinearPhase", 1 }, "Linear phase", false));
    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "eqEnabled", 1 }, "Equalizer", true));
    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "eqDynamic", 1 }, "Dynamic EQ", false));

    const std::array<float, eqBandCount> defaults {
        32.0f, 55.0f, 90.0f, 150.0f, 250.0f, 400.0f, 650.0f, 1000.0f,
        1600.0f, 2500.0f, 4000.0f, 6300.0f, 10000.0f, 15000.0f, 19000.0f
    };
    for (int band = 0; band < eqBandCount; ++band)
    {
        layout.add(std::make_unique<Float>(
            juce::ParameterID { eqId("eqFreq", band), 1 }, "EQ frequency " + juce::String(band + 1),
            range(20.0f, 20000.0f, 0.01f), defaults[static_cast<size_t>(band)], label("Hz")));
        layout.add(std::make_unique<Float>(
            juce::ParameterID { eqId("eqGain", band), 1 }, "EQ gain " + juce::String(band + 1),
            range(-18.0f, 18.0f, 0.01f), 0.0f, label("dB")));
        layout.add(std::make_unique<Float>(
            juce::ParameterID { eqId("eqQ", band), 1 }, "EQ Q " + juce::String(band + 1),
            range(0.1f, 12.0f, 0.01f), 0.85f, label("Q")));
        layout.add(std::make_unique<Bool>(
            juce::ParameterID { eqId("eqDyn", band), 1 }, "EQ dynamic " + juce::String(band + 1), false));
    }

    layout.add(std::make_unique<Float>(
        juce::ParameterID { "targetLufs", 1 }, "Target loudness",
        range(-24.0f, -8.0f, 0.1f), -14.0f, label("LUFS")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "volume", 1 }, "Volume",
        range(-24.0f, 24.0f, 0.01f), 0.0f, label("dB")));

    layout.add(std::make_unique<Float>(
        juce::ParameterID { "saturationMix", 1 }, "Saturation mix",
        range(0.0f, 100.0f, 0.1f), 0.0f, label("%")));
    layout.add(std::make_unique<Choice>(
        juce::ParameterID { "saturationPreset", 1 }, "Saturation preset",
        juce::StringArray { "Warm", "Cold / Airy", "Distortion", "Tube" }, 0));

    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "stereoEnabled", 1 }, "Stereo imager", true));
    const std::array<float, 3> crossoverDefaults { 120.0f, 1000.0f, 6000.0f };
    for (int band = 0; band < stereoBandCount; ++band)
    {
        if (band < stereoBandCount - 1)
            layout.add(std::make_unique<Float>(
                juce::ParameterID { stereoId("stereoXover", band), 1 },
                "Stereo crossover " + juce::String(band + 1),
                range(40.0f, 18000.0f, 0.1f), crossoverDefaults[static_cast<size_t>(band)], label("Hz")));
        layout.add(std::make_unique<Float>(
            juce::ParameterID { stereoId("stereoWidth", band), 1 },
            "Stereo width " + juce::String(band + 1), range(0.0f, 200.0f, 0.1f), 100.0f, label("%")));
        layout.add(std::make_unique<Choice>(
            juce::ParameterID { stereoId("stereoPreset", band), 1 },
            "Stereo preset " + juce::String(band + 1),
            juce::StringArray { "Warm", "Cold / Airy", "Distortion", "Tube" }, 0));
    }

    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "compressorEnabled", 1 }, "OPTO compressor", true));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compAttack", 1 }, "OPTO attack", range(1.0f, 200.0f, 0.1f), 35.0f, label("ms")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compRelease", 1 }, "OPTO release", range(20.0f, 2000.0f, 0.1f), 280.0f, label("ms")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compRatio", 1 }, "OPTO ratio", range(1.0f, 20.0f, 0.01f), 3.0f, label(":1")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compInput", 1 }, "OPTO input", range(-24.0f, 24.0f, 0.01f), 0.0f, label("dB")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compOutput", 1 }, "OPTO output", range(-24.0f, 24.0f, 0.01f), 0.0f, label("dB")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "compThreshold", 1 }, "OPTO threshold", range(-48.0f, 0.0f, 0.1f), -18.0f, label("dB")));

    layout.add(std::make_unique<Bool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

bool NorthstarMasteringAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void NorthstarMasteringAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maximumBlockSize = samplesPerBlock;
    compressorEnvelope = 0.0f;
    smoothPeak = smoothInputLoudness = smoothOutputLoudness = -60.0f;
    lastGainReduction = 0.0f;
    dynamicEnvelopes.fill(0.0f);
    stereoLowStates = {};
    spectrumFifo.fill(0.0f);
    spectrumWork.fill(0.0f);
    spectrumFifoPosition = 0;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1 };
    for (auto& band : eqFilters)
        for (auto& filter : band)
        {
            filter.prepare(spec);
            filter.reset();
        }
    for (auto& filter : linearEqFilters)
    {
        filter.prepare(spec);
        filter.reset();
    }
    linearEqCoefficients = new juce::dsp::FIR::Coefficients<float>(
        std::array<float, linearPhaseTaps> { 1.0f }.data(), linearPhaseTaps);
    for (auto& filter : linearEqFilters)
        filter.coefficients = linearEqCoefficients;
    linearPhaseWork.fill(0.0f);
    lastLinearPhaseSignature = -100000.0f;
    for (auto& stage : loudnessFilters)
        for (auto& filter : stage)
        {
            filter.prepare(spec);
            filter.reset();
        }
    for (auto& stage : crossoverFilters)
        for (auto& filter : stage)
        {
            filter.prepare(spec);
            filter.reset();
        }

    for (size_t channel = 0; channel < 2; ++channel)
    {
        loudnessFilters[0][channel].coefficients =
            Coefficients::makeHighPass(sampleRate, juce::jmin(38.0, sampleRate * 0.2), 0.5f);
        loudnessFilters[1][channel].coefficients =
            Coefficients::makeHighShelf(sampleRate, juce::jmin(1682.0, sampleRate * 0.4),
                                        0.707f, juce::Decibels::decibelsToGain(4.0f));
        loudnessFilters[2][channel].coefficients =
            Coefficients::makeLowPass(sampleRate, juce::jmin(20000.0, sampleRate * 0.45), 0.707f);
    }

    updateCrossovers();
    updateEQFilters();
    analysisRunning.store(false);
    analysisComplete.store(false);
    analysisProgress.store(0.0f);
}

void NorthstarMasteringAudioProcessor::releaseResources()
{
}

void NorthstarMasteringAudioProcessor::requestAnalysis()
{
    analysisRequested.store(true);
}

float NorthstarMasteringAudioProcessor::toDb(float value) noexcept
{
    return 20.0f * std::log10(std::max(value, safeFloor));
}

void NorthstarMasteringAudioProcessor::updateEQFilters()
{
    for (int band = 0; band < eqBandCount; ++band)
    {
        const auto frequency = juce::jlimit(20.0f, static_cast<float>(currentSampleRate * 0.45),
                                            parameters.getRawParameterValue(eqId("eqFreq", band))->load());
        const auto gain = parameters.getRawParameterValue(eqId("eqGain", band))->load();
        const auto q = parameters.getRawParameterValue(eqId("eqQ", band))->load();
        const auto coefficients = Coefficients::makePeakFilter(
            currentSampleRate, frequency, q, juce::Decibels::decibelsToGain(gain));
        for (auto& filter : eqFilters[static_cast<size_t>(band)])
            filter.coefficients = coefficients;
    }
}

void NorthstarMasteringAudioProcessor::updateLinearPhaseFilters()
{
    float signature = 0.0f;
    for (int band = 0; band < eqBandCount; ++band)
    {
        signature += parameters.getRawParameterValue(eqId("eqFreq", band))->load() * 0.0001f;
        signature += parameters.getRawParameterValue(eqId("eqGain", band))->load();
        signature += parameters.getRawParameterValue(eqId("eqQ", band))->load() * 0.01f;
    }
    if (std::abs(signature - lastLinearPhaseSignature) < 0.0001f)
        return;

    constexpr int fftSize = 1024;
    constexpr int halfSize = fftSize / 2;
    constexpr float delay = static_cast<float>(linearPhaseTaps - 1) * 0.5f;
    linearPhaseWork.fill(0.0f);
    for (int bin = 0; bin <= halfSize; ++bin)
    {
        const auto frequency = juce::jmax(
            20.0f, static_cast<float>(bin) * static_cast<float>(currentSampleRate) / fftSize);
        auto gainDb = 0.0f;
        for (int band = 0; band < eqBandCount; ++band)
        {
            const auto centre = parameters.getRawParameterValue(eqId("eqFreq", band))->load();
            const auto gain = parameters.getRawParameterValue(eqId("eqGain", band))->load();
            const auto q = parameters.getRawParameterValue(eqId("eqQ", band))->load();
            const auto octaveDistance = std::log2(frequency / juce::jmax(20.0f, centre));
            const auto width = juce::jmax(0.08f, 0.85f / q);
            gainDb += gain * std::exp(-0.5f * octaveDistance * octaveDistance / (width * width));
        }
        const auto magnitude = juce::Decibels::decibelsToGain(gainDb);
        const auto phase = -2.0f * pi * static_cast<float>(bin) * delay / fftSize;
        linearPhaseWork[static_cast<size_t>(bin * 2)] = magnitude * std::cos(phase);
        linearPhaseWork[static_cast<size_t>(bin * 2 + 1)] = magnitude * std::sin(phase);
        if (bin > 0 && bin < halfSize)
        {
            const auto mirror = fftSize - bin;
            linearPhaseWork[static_cast<size_t>(mirror * 2)] = linearPhaseWork[static_cast<size_t>(bin * 2)];
            linearPhaseWork[static_cast<size_t>(mirror * 2 + 1)] =
                -linearPhaseWork[static_cast<size_t>(bin * 2 + 1)];
        }
    }
    linearPhaseFft.performRealOnlyInverseTransform(linearPhaseWork.data());

    std::array<float, linearPhaseTaps> coefficients {};
    for (int index = 0; index < linearPhaseTaps; ++index)
    {
        const auto window = 0.5f - 0.5f * std::cos(
            2.0f * pi * static_cast<float>(index) / static_cast<float>(linearPhaseTaps - 1));
        coefficients[static_cast<size_t>(index)] =
            linearPhaseWork[static_cast<size_t>(index)] * window;
    }
    linearEqCoefficients = new juce::dsp::FIR::Coefficients<float>(
        coefficients.data(), coefficients.size());
    for (auto& filter : linearEqFilters)
        filter.coefficients = linearEqCoefficients;
    lastLinearPhaseSignature = signature;
}

void NorthstarMasteringAudioProcessor::updateCompressorCoefficients()
{
    const auto attack = parameters.getRawParameterValue("compAttack")->load();
    const auto release = parameters.getRawParameterValue("compRelease")->load();
    juce::ignoreUnused(attack, release);
}

void NorthstarMasteringAudioProcessor::updateCrossovers()
{
    const auto maximumFrequency = static_cast<float>(currentSampleRate * 0.45);
    auto previousFrequency = 40.0f;
    for (int index = 0; index < stereoBandCount - 1; ++index)
    {
        const auto requested = parameters.getRawParameterValue(
            stereoId("stereoXover", index))->load();
        const auto remainingCrossovers = stereoBandCount - 2 - index;
        const auto maximumForBand = maximumFrequency - static_cast<float>(remainingCrossovers) * 20.0f;
        const auto frequency = juce::jlimit(previousFrequency + 20.0f,
                                            maximumForBand, requested);
        crossoverFrequencies[static_cast<size_t>(index)] = frequency;
        for (auto& filter : crossoverFilters[static_cast<size_t>(index)])
            filter.coefficients = Coefficients::makeLowPass(
                currentSampleRate, juce::jlimit(30.0f, maximumFrequency, frequency),
                0.707f);
        previousFrequency = frequency;
    }
}

float NorthstarMasteringAudioProcessor::applySaturation(float sample, float mix, int preset) const noexcept
{
    if (mix <= 0.0001f)
        return sample;

    const std::array<float, 4> drive { 1.35f, 1.12f, 3.8f, 1.85f };
    const std::array<float, 4> bias { 0.02f, 0.0f, 0.05f, 0.09f };
    const auto index = juce::jlimit(0, 3, preset);
    auto shaped = std::tanh(sample * drive[static_cast<size_t>(index)] + bias[static_cast<size_t>(index)]);
    if (index == 1)
        shaped = 0.92f * shaped + 0.08f * sample;
    if (index == 2)
        shaped = std::copysign(std::pow(std::abs(shaped), 0.72f), shaped);
    if (index == 3)
        shaped = std::tanh(sample * 1.6f) * 0.94f + sample * 0.06f;
    return sample + (shaped - sample) * juce::jlimit(0.0f, 1.0f, mix);
}

void NorthstarMasteringAudioProcessor::applyStereoImage(float& left, float& right)
{
    if (parameters.getRawParameterValue("stereoEnabled")->load() < 0.5f)
        return;

    std::array<float, stereoBandCount> leftBands {};
    std::array<float, stereoBandCount> rightBands {};
    float leftRemainder = left;
    float rightRemainder = right;
    for (int band = 0; band < stereoBandCount - 1; ++band)
    {
        const auto stateIndex = static_cast<size_t>(band);
        const auto coeff = juce::jlimit(0.001f, 0.999f,
            std::exp(-2.0f * pi * crossoverFrequencies[stateIndex]
                     / static_cast<float>(currentSampleRate)));
        stereoLowStates[0][stateIndex] = coeff * stereoLowStates[0][stateIndex]
            + (1.0f - coeff) * leftRemainder;
        stereoLowStates[1][stateIndex] = coeff * stereoLowStates[1][stateIndex]
            + (1.0f - coeff) * rightRemainder;
        leftBands[stateIndex] = stereoLowStates[0][stateIndex];
        rightBands[stateIndex] = stereoLowStates[1][stateIndex];
        leftRemainder -= leftBands[stateIndex];
        rightRemainder -= rightBands[stateIndex];
    }
    leftBands[stereoBandCount - 1] = leftRemainder;
    rightBands[stereoBandCount - 1] = rightRemainder;

    left = right = 0.0f;
    static constexpr std::array<float, stereoBandCount> presetWidth {
        1.0f, 1.08f, 1.18f, 1.02f
    };
    for (int band = 0; band < stereoBandCount; ++band)
    {
        const auto width = parameters.getRawParameterValue(stereoId("stereoWidth", band))->load() * 0.01f;
        const auto preset = static_cast<int>(parameters.getRawParameterValue(
            stereoId("stereoPreset", band))->load());
        // Imager presets change the amount of side signal only. They must not
        // run a waveshaper on every band: that creates aliasing and can make a
        // neutral 100% width setting sound coloured or phasey.
        const auto presetWidthScale = presetWidth[static_cast<size_t>(juce::jlimit(0, 3, preset))];
        const auto mid = (leftBands[static_cast<size_t>(band)] + rightBands[static_cast<size_t>(band)]) * 0.5f;
        const auto side = (leftBands[static_cast<size_t>(band)] - rightBands[static_cast<size_t>(band)])
            * width * presetWidthScale;
        left += mid + side;
        right += mid - side;
    }
}

void NorthstarMasteringAudioProcessor::finishAnalysis()
{
    const auto samples = std::max<int64_t>(1, analyzedSamples);
    const auto meanEnergy = std::max(analysisEnergy / static_cast<double>(samples), 1.0e-12);
    const auto inputLufs = static_cast<float>(-0.691 + 10.0 * std::log10(meanEnergy));
    loudnessEstimate.store(inputLufs);
    analysisComplete.store(true);
    analysisRunning.store(false);
    analysisProgress.store(1.0f);
    collectingAnalysis = false;

    const auto totalBandEnergy = std::max(
        analysisBandEnergy[0] + analysisBandEnergy[1] + analysisBandEnergy[2], 1.0e-12);
    const auto targetShares = std::array<float, 3> { 0.25f, 0.55f, 0.20f };
    const auto isAutomatic = parameters.getRawParameterValue("eqMode")->load() < 0.5f;
    for (int index = 0; index < 3 && isAutomatic; ++index)
    {
        const auto observed = static_cast<float>(analysisBandEnergy[static_cast<size_t>(index)]
                                                  / totalBandEnergy);
        const auto correction = juce::jlimit(-5.0f, 5.0f,
            10.0f * std::log10(targetShares[static_cast<size_t>(index)]
                                / std::max(observed, 0.02f)));
        const auto band = index == 0 ? 0 : (index == 1 ? 6 : 12);
        setEQGain(band, correction * 0.38f);
    }
    if (isAutomatic)
        parameters.getParameter("eqMode")->setValueNotifyingHost(0.0f);
}

void NorthstarMasteringAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = getTotalNumInputChannels();
    const auto channels = juce::jmin(inputChannels, 2);
    for (int channel = channels; channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (analysisRequested.exchange(false))
    {
        collectingAnalysis = true;
        analyzedSamples = 0;
        analysisEnergy = 0.0;
        analysisPeak = 0.0;
        analysisBandEnergy = {};
        analysisProgress.store(0.0f);
        analysisComplete.store(false);
        analysisRunning.store(true);
    }

    updateEQFilters();
    updateCrossovers();
    const auto linearPhase = parameters.getRawParameterValue("eqLinearPhase")->load() > 0.5f;
    if (linearPhase)
        updateLinearPhaseFilters();
    const auto bypass = parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const auto eqOn = parameters.getRawParameterValue("eqEnabled")->load() > 0.5f;
    const auto eqDynamicGlobal = parameters.getRawParameterValue("eqDynamic")->load() > 0.5f;
    const auto saturationMix = parameters.getRawParameterValue("saturationMix")->load() * 0.01f;
    const auto saturationPreset = static_cast<int>(
        parameters.getRawParameterValue("saturationPreset")->load());
    const auto compressorOn = parameters.getRawParameterValue("compressorEnabled")->load() > 0.5f;
    const auto compInput = parameters.getRawParameterValue("compInput")->load();
    const auto compOutput = parameters.getRawParameterValue("compOutput")->load();
    const auto compThreshold = parameters.getRawParameterValue("compThreshold")->load();
    const auto compRatio = parameters.getRawParameterValue("compRatio")->load();
    const auto attackCoeff = std::exp(-1.0f /
        (static_cast<float>(currentSampleRate)
         * parameters.getRawParameterValue("compAttack")->load() * 0.001f));
    const auto releaseCoeff = std::exp(-1.0f /
        (static_cast<float>(currentSampleRate)
         * parameters.getRawParameterValue("compRelease")->load() * 0.001f));
    const auto volumeGain = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue("volume")->load());
    const auto targetLufs = parameters.getRawParameterValue("targetLufs")->load();
    const auto learnedMakeup = analysisComplete.load()
        ? juce::jlimit(-12.0f, 12.0f, (targetLufs - loudnessEstimate.load()) * 0.72f)
        : 0.0f;
    const auto autoGain = juce::Decibels::decibelsToGain(learnedMakeup);
    double blockInputEnergy = 0.0;
    double blockOutputEnergy = 0.0;
    float blockPeak = 0.0f;
    float blockReduction = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float left = channels > 0 ? buffer.getSample(0, sample) : 0.0f;
        float right = channels > 1 ? buffer.getSample(1, sample) : left;
        const auto inputMono = (left + right) * 0.5f;
        const auto detector = juce::jmax(std::abs(left), std::abs(right));
        auto weightedLeft = loudnessFilters[0][0].processSample(left);
        weightedLeft = loudnessFilters[1][0].processSample(weightedLeft);
        auto weightedRight = loudnessFilters[0][1].processSample(right);
        weightedRight = loudnessFilters[1][1].processSample(weightedRight);
        const auto weightedEnergy = 0.5f * (weightedLeft * weightedLeft
                                            + weightedRight * weightedRight);
        blockInputEnergy += weightedEnergy;
        if (collectingAnalysis)
        {
            analysisEnergy += weightedEnergy;
            analysisPeak = std::max(analysisPeak, static_cast<double>(detector));
            const auto low = detector * 0.55f;
            const auto mid = detector * 0.35f;
            const auto high = detector * 0.20f;
            analysisBandEnergy[0] += low * low;
            analysisBandEnergy[1] += mid * mid;
            analysisBandEnergy[2] += high * high;
            ++analyzedSamples;
        }

        left = applySaturation(left, saturationMix, saturationPreset);
        right = applySaturation(right, saturationMix, saturationPreset);
        if (eqOn)
        {
            if (linearPhase)
            {
                left = linearEqFilters[0].processSample(left);
                right = linearEqFilters[1].processSample(right);
            }
            else
            {
                for (int band = 0; band < eqBandCount; ++band)
                {
                    const auto bandIndex = static_cast<size_t>(band);
                    left = eqFilters[bandIndex][0].processSample(left);
                    right = eqFilters[bandIndex][1].processSample(right);
                    const auto dynamic = eqDynamicGlobal
                        || parameters.getRawParameterValue(eqId("eqDyn", band))->load() > 0.5f;
                    if (dynamic)
                    {
                        const auto energy = 0.5f * (std::abs(left) + std::abs(right));
                        const auto& gainParam = parameters.getRawParameterValue(eqId("eqGain", band));
                        const auto targetReduction = energy > 0.18f
                            ? juce::jlimit(0.0f, 1.0f, (energy - 0.18f) * 2.5f) : 0.0f;
                        dynamicEnvelopes[bandIndex] = 0.995f * dynamicEnvelopes[bandIndex]
                            + 0.005f * targetReduction;
                        const auto dynamicGain = juce::Decibels::decibelsToGain(
                            -std::abs(gainParam->load()) * dynamicEnvelopes[bandIndex] * 0.55f);
                        left *= dynamicGain;
                        right *= dynamicGain;
                    }
                }
            }
        }

        applyStereoImage(left, right);
        const auto compDetector = toDb(juce::jmax(std::abs(left), std::abs(right)))
            + compInput;
        const auto detectorGain = juce::Decibels::decibelsToGain(compDetector);
        compressorEnvelope = detectorGain > compressorEnvelope
            ? attackCoeff * compressorEnvelope + (1.0f - attackCoeff) * detectorGain
            : releaseCoeff * compressorEnvelope + (1.0f - releaseCoeff) * detectorGain;
        const auto envelopeDb = toDb(compressorEnvelope);
        const auto over = juce::jmax(0.0f, envelopeDb - compThreshold);
        const auto reductionDb = compressorOn ? over * (1.0f - 1.0f / compRatio) : 0.0f;
        const auto compGain = juce::Decibels::decibelsToGain(-reductionDb);
        blockReduction = juce::jmax(blockReduction, reductionDb);

        auto outputGain = compGain * autoGain * volumeGain
            * juce::Decibels::decibelsToGain(compOutput);
        left *= outputGain;
        right *= outputGain;
        // Keep the float signal transparent. Hard-clipping here made the
        // supposedly clean loudness/volume stage create distortion whenever
        // the user raised the gain. The host/output format can handle float
        // headroom and the peak meter reports the actual post-gain level.
        blockOutputEnergy += 0.5 * (left * left + right * right);
        blockPeak = juce::jmax(blockPeak, std::abs(left), std::abs(right));
        if (!bypass)
        {
            buffer.setSample(0, sample, left);
            if (channels > 1)
                buffer.setSample(1, sample, right);
        }

        // Keep the analyzer pre-processing so the EQ nodes can be positioned
        // against the material entering the mastering chain.
        spectrumFifo[static_cast<size_t>(spectrumFifoPosition++)] = inputMono;
        if (spectrumFifoPosition == spectrumSize)
        {
            std::copy(spectrumFifo.begin(), spectrumFifo.end(), spectrumWork.begin());
            spectrumWindow.multiplyWithWindowingTable(spectrumWork.data(), spectrumSize);
            spectrumFft.performFrequencyOnlyForwardTransform(spectrumWork.data());
            std::array<float, spectrumBinCount> magnitudes {};
            std::array<int, spectrumBinCount> counts {};
            for (int bin = 1; bin <= spectrumSize / 2; ++bin)
            {
                const auto frequency = static_cast<float>(bin) * static_cast<float>(currentSampleRate)
                    / static_cast<float>(spectrumSize);
                if (frequency < 20.0f || frequency > 20000.0f)
                    continue;
                const auto position = juce::jlimit(0, spectrumBinCount - 1, static_cast<int>(
                    std::log(frequency / 20.0f) / std::log(1000.0f) * spectrumBinCount));
                magnitudes[static_cast<size_t>(position)] += spectrumWork[static_cast<size_t>(bin)];
                ++counts[static_cast<size_t>(position)];
            }
            std::array<float, spectrumBinCount> normalizedBins {};
            for (int index = 0; index < spectrumBinCount; ++index)
            {
                const auto average = counts[static_cast<size_t>(index)] > 0
                    ? magnitudes[static_cast<size_t>(index)] / counts[static_cast<size_t>(index)] : 0.0f;
                const auto db = 20.0f * std::log10(std::max(average / 2048.0f, 1.0e-6f));
                normalizedBins[static_cast<size_t>(index)] =
                    juce::jlimit(0.0f, 1.0f, (db + 72.0f) / 60.0f);
            }
            for (int index = 0; index < spectrumBinCount; ++index)
            {
                const auto previous = normalizedBins[static_cast<size_t>(juce::jmax(0, index - 1))];
                const auto current = normalizedBins[static_cast<size_t>(index)];
                const auto next = normalizedBins[static_cast<size_t>(
                    juce::jmin(spectrumBinCount - 1, index + 1))];
                const auto smoothed = 0.20f * previous + 0.60f * current + 0.20f * next;
                auto& binValue = spectrumBins[static_cast<size_t>(index)];
                binValue.store(0.80f * binValue.load() + 0.20f * smoothed);
            }
            spectrumFifoPosition = 0;
        }
    }

    const auto sampleCount = juce::jmax(1, buffer.getNumSamples());
    const auto inputLufs = -0.691f + 10.0f * std::log10(
        std::max(static_cast<float>(blockInputEnergy / sampleCount), 1.0e-12f));
    const auto outputLufs = -0.691f + 10.0f * std::log10(
        std::max(static_cast<float>(blockOutputEnergy / sampleCount), 1.0e-12f));
    smoothInputLoudness = smoothInputLoudness < -59.0f
        ? inputLufs : 0.92f * smoothInputLoudness + 0.08f * inputLufs;
    smoothOutputLoudness = smoothOutputLoudness < -59.0f
        ? outputLufs : 0.92f * smoothOutputLoudness + 0.08f * outputLufs;
    smoothPeak = smoothPeak < -59.0f
        ? toDb(blockPeak) : 0.84f * smoothPeak + 0.16f * toDb(blockPeak);
    loudnessEstimate.store(smoothInputLoudness);
    outputLoudness.store(smoothOutputLoudness);
    peakDb.store(smoothPeak);
    gainReductionDb.store(blockReduction);

    if (collectingAnalysis)
    {
        analysisProgress.store(juce::jlimit(0.0f, 1.0f,
            static_cast<float>(analyzedSamples)
            / static_cast<float>(currentSampleRate * analysisDurationSeconds)));
        if (analyzedSamples >= static_cast<int64_t>(currentSampleRate * analysisDurationSeconds))
            finishAnalysis();
    }
}

void NorthstarMasteringAudioProcessor::copySpectrum(
    std::array<float, spectrumBinCount>& destination) const noexcept
{
    for (size_t index = 0; index < destination.size(); ++index)
        destination[index] = spectrumBins[index].load();
}

float NorthstarMasteringAudioProcessor::getEQFrequency(int band) const noexcept
{
    return parameters.getRawParameterValue(eqId("eqFreq", juce::jlimit(0, eqBandCount - 1, band)))->load();
}

float NorthstarMasteringAudioProcessor::getEQGain(int band) const noexcept
{
    return parameters.getRawParameterValue(eqId("eqGain", juce::jlimit(0, eqBandCount - 1, band)))->load();
}

float NorthstarMasteringAudioProcessor::getEQQ(int band) const noexcept
{
    return parameters.getRawParameterValue(eqId("eqQ", juce::jlimit(0, eqBandCount - 1, band)))->load();
}

bool NorthstarMasteringAudioProcessor::getEQDynamic(int band) const noexcept
{
    return parameters.getRawParameterValue(eqId("eqDyn", juce::jlimit(0, eqBandCount - 1, band)))->load() > 0.5f;
}

void NorthstarMasteringAudioProcessor::setEQFrequency(int band, float value)
{
    const auto index = juce::jlimit(0, eqBandCount - 1, band);
    auto* parameter = parameters.getParameter(eqId("eqFreq", index));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(juce::jlimit(20.0f, 20000.0f, value)));
}

void NorthstarMasteringAudioProcessor::setEQGain(int band, float value)
{
    const auto index = juce::jlimit(0, eqBandCount - 1, band);
    auto* parameter = parameters.getParameter(eqId("eqGain", index));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(juce::jlimit(-18.0f, 18.0f, value)));
}

void NorthstarMasteringAudioProcessor::setEQQ(int band, float value)
{
    const auto index = juce::jlimit(0, eqBandCount - 1, band);
    auto* parameter = parameters.getParameter(eqId("eqQ", index));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(juce::jlimit(0.1f, 12.0f, value)));
}

void NorthstarMasteringAudioProcessor::setEQDynamic(int band, bool enabled)
{
    const auto index = juce::jlimit(0, eqBandCount - 1, band);
    parameters.getParameter(eqId("eqDyn", index))->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
}

void NorthstarMasteringAudioProcessor::resetEQToAuto()
{
    for (int band = 0; band < eqBandCount; ++band)
    {
        setEQGain(band, 0.0f);
        setEQDynamic(band, false);
    }
    parameters.getParameter("eqMode")->setValueNotifyingHost(0.0f);
}

void NorthstarMasteringAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void NorthstarMasteringAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor* NorthstarMasteringAudioProcessor::createEditor()
{
    return new NorthstarMasteringAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorthstarMasteringAudioProcessor();
}