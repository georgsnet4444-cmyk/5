#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>

class NorthstarMasteringAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit NorthstarMasteringAudioProcessorEditor(NorthstarMasteringAudioProcessor&);
    ~NorthstarMasteringAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class Page { eq, loudness, saturation, stereo, compressor };

    class Meter final : public juce::Component
    {
    public:
        void setLevels(float peak, float loudness, float gainReduction);
        void paint(juce::Graphics&) override;
    private:
        float peakLevel = 0.0f;
        float loudnessLevel = 0.0f;
        float reductionLevel = 0.0f;
    };

    class Spectrum final : public juce::Component
    {
    public:
        void setValues(const std::array<float, 64>& values);
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 64> bins {};
    };

    class EQGraph final : public juce::Component
    {
    public:
        explicit EQGraph(NorthstarMasteringAudioProcessor& processorToUse);
        void setSelectedBand(int band);
        void setSpectrum(const std::array<float, 64>& values);
        int getSelectedBand() const noexcept { return selectedBand; }
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        std::function<void(int)> onBandSelected;
    private:
        int bandAtPosition(juce::Point<float> position) const;
        void updateBandFromPosition(juce::Point<float> position);
        float xForFrequency(float frequency) const;
        float yForGain(float gain) const;
        float frequencyForX(float x) const;
        float gainForY(float y) const;
        NorthstarMasteringAudioProcessor& processor;
        int selectedBand = 0;
        bool dragging = false;
        std::array<float, 64> spectrumBins {};
    };

    void timerCallback() override;
    void styleSlider(juce::Slider&, const juce::String& suffix);
    void styleButton(juce::TextButton&, bool toggle = true);
    void styleToggle(juce::ToggleButton&);
    void setPage(Page);
    void updateEQControls();
    void updateLabels();

    NorthstarMasteringAudioProcessor& processor;
    Page page = Page::eq;
    int selectedEQBand = 0;
    bool updatingEQControls = false;

    juce::Label brandLabel;
    juce::Label subtitleLabel;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label reductionLabel;
    juce::Label statusLabel;

    juce::TextButton eqTab { "EQUALIZATION" };
    juce::TextButton loudnessTab { "LOUDNESS" };
    juce::TextButton saturationTab { "SATURATION" };
    juce::TextButton stereoTab { "STEREO IMAGER" };
    juce::TextButton compressorTab { "OPTO COMPRESSOR" };
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton analyzeButton { "ANALYZE 10 SEC" };

    juce::ComboBox eqModeSelector;
    juce::ToggleButton eqPhaseButton { "LINEAR PHASE" };
    juce::ToggleButton eqEnabledButton { "EQ ON" };
    juce::ToggleButton eqDynamicButton { "DYNAMIC" };
    juce::TextButton eqAutoButton { "AUTO RESET" };
    juce::Slider eqFrequencySlider;
    juce::Slider eqGainSlider;
    juce::Slider eqQSlider;
    juce::Label eqBandLabel;

    juce::Slider targetSlider;
    juce::Slider volumeSlider;
    juce::Label targetValueLabel;
    juce::Label volumeValueLabel;
    juce::TextButton learnButton { "LEARN 10 SEC" };

    juce::Slider saturationMixSlider;
    juce::ComboBox saturationPresetSelector;

    std::array<juce::Slider, 3> crossoverSliders;
    std::array<juce::Slider, 4> widthSliders;
    std::array<juce::ComboBox, 4> stereoPresetSelectors;
    juce::ToggleButton stereoEnabledButton { "IMAGER ON" };

    juce::Slider compAttackSlider;
    juce::Slider compReleaseSlider;
    juce::Slider compRatioSlider;
    juce::Slider compInputSlider;
    juce::Slider compOutputSlider;
    juce::Slider compThresholdSlider;
    juce::ToggleButton compressorEnabledButton { "OPTO ON" };

    Meter meter;
    Spectrum spectrum;
    EQGraph eqGraph;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> targetAttachment;
    std::unique_ptr<SliderAttachment> volumeAttachment;
    std::unique_ptr<SliderAttachment> saturationMixAttachment;
    std::unique_ptr<ComboAttachment> saturationPresetAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 3> crossoverAttachments;
    std::array<std::unique_ptr<SliderAttachment>, 4> widthAttachments;
    std::array<std::unique_ptr<ComboAttachment>, 4> stereoPresetAttachments;
    std::unique_ptr<SliderAttachment> compAttackAttachment;
    std::unique_ptr<SliderAttachment> compReleaseAttachment;
    std::unique_ptr<SliderAttachment> compRatioAttachment;
    std::unique_ptr<SliderAttachment> compInputAttachment;
    std::unique_ptr<SliderAttachment> compOutputAttachment;
    std::unique_ptr<SliderAttachment> compThresholdAttachment;
    std::unique_ptr<ComboAttachment> eqModeAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> eqPhaseAttachment;
    std::unique_ptr<ButtonAttachment> eqEnabledAttachment;
    std::unique_ptr<ButtonAttachment> stereoEnabledAttachment;
    std::unique_ptr<ButtonAttachment> compressorEnabledAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NorthstarMasteringAudioProcessorEditor)
};