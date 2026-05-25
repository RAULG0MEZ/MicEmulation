#pragma once

#include <JuceHeader.h>
#include "EQModel.h"

class RodeM2ToSlateML1AudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    RodeM2ToSlateML1AudioProcessor();
    ~RodeM2ToSlateML1AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getBlendPercent() const;
    float getBlend01() const;
    int getSourceMicIndex() const;
    juce::String getSourceMicName() const;
    double getAnalysisSampleRate() const;

    APVTS parameters;

private:
    struct DynamicBandState
    {
        juce::dsp::IIR::Filter<float> bandpass;
        float envelope = 0.0f;
        float attackCoefficient = 0.0f;
        float releaseCoefficient = 0.0f;
    };

    static APVTS::ParameterLayout createParameterLayout();

    void ensureFilterCount (int channelCount);
    void rebuildFilters (float blend01, int sourceMicIndex, bool resetState);
    void processDynamicBands (float& sample,
                              std::vector<DynamicBandState>& channelDynamicBands,
                              const EQModel::Profile& profile,
                              float blend01);

    std::atomic<float>* blendParameter = nullptr;
    std::atomic<float>* sourceMicParameter = nullptr;

    std::vector<std::vector<juce::dsp::IIR::Filter<float>>> filters;
    std::vector<std::vector<DynamicBandState>> dynamicBandStates;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> blendSmoother;
    double activeSampleRate = 48000.0;
    float lastFilterBlend = -1.0f;
    int lastSourceMicIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RodeM2ToSlateML1AudioProcessor)
};
