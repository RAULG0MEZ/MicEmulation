#include "PluginProcessor.h"
#include "PluginEditor.h"

RodeM2ToSlateML1AudioProcessor::RodeM2ToSlateML1AudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    blendParameter = parameters.getRawParameterValue ("blend");
    sourceMicParameter = parameters.getRawParameterValue ("sourceMic");
}

RodeM2ToSlateML1AudioProcessor::APVTS::ParameterLayout RodeM2ToSlateML1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "blend", 1 },
        "Blend",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.01f, 1.0f },
        100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (value, 1) + "%";
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.").getFloatValue();
            })));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "sourceMic", 1 },
        "Source Mic",
        EQModel::getProfileNames(),
        0));

    return { params.begin(), params.end() };
}

const juce::String RodeM2ToSlateML1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RodeM2ToSlateML1AudioProcessor::acceptsMidi() const
{
    return false;
}

bool RodeM2ToSlateML1AudioProcessor::producesMidi() const
{
    return false;
}

bool RodeM2ToSlateML1AudioProcessor::isMidiEffect() const
{
    return false;
}

double RodeM2ToSlateML1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RodeM2ToSlateML1AudioProcessor::getNumPrograms()
{
    return 1;
}

int RodeM2ToSlateML1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void RodeM2ToSlateML1AudioProcessor::setCurrentProgram (int)
{
}

const juce::String RodeM2ToSlateML1AudioProcessor::getProgramName (int)
{
    return {};
}

void RodeM2ToSlateML1AudioProcessor::changeProgramName (int, const juce::String&)
{
}

void RodeM2ToSlateML1AudioProcessor::prepareToPlay (double sampleRate, int)
{
    activeSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    blendSmoother.reset (activeSampleRate, 0.035);
    blendSmoother.setCurrentAndTargetValue (getBlend01());
    ensureFilterCount (juce::jmax (1, getTotalNumOutputChannels()));
    rebuildFilters (getBlend01(), getSourceMicIndex(), true);
}

void RodeM2ToSlateML1AudioProcessor::releaseResources()
{
    for (auto& channelFilters : filters)
        for (auto& filter : channelFilters)
            filter.reset();
}

bool RodeM2ToSlateML1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
}

void RodeM2ToSlateML1AudioProcessor::ensureFilterCount (int channelCount)
{
    const auto safeChannelCount = juce::jmax (1, channelCount);

    if (static_cast<int> (filters.size()) == safeChannelCount)
        return;

    filters.clear();
    filters.resize (static_cast<size_t> (safeChannelCount));

    for (auto& channelFilters : filters)
        channelFilters.resize (EQModel::bandCount);

    lastFilterBlend = -1.0f;
    lastSourceMicIndex = -1;
}

void RodeM2ToSlateML1AudioProcessor::rebuildFilters (float blend01, int sourceMicIndex, bool resetState)
{
    const auto safeBlend = juce::jlimit (0.0f, 2.0f, blend01);
    const auto safeSourceMicIndex = EQModel::getValidProfileIndex (sourceMicIndex);
    const auto& profile = EQModel::getProfile (safeSourceMicIndex);

    for (auto& channelFilters : filters)
    {
        for (size_t i = 0; i < profile.bands.size(); ++i)
        {
            auto coefficients = EQModel::makeCoefficients (profile.bands[i], activeSampleRate, safeBlend);

            if (coefficients != nullptr)
                *channelFilters[i].coefficients = *coefficients;

            if (resetState)
                channelFilters[i].reset();
        }
    }

    lastFilterBlend = safeBlend;
    lastSourceMicIndex = safeSourceMicIndex;
}

void RodeM2ToSlateML1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    ensureFilterCount (buffer.getNumChannels());

    const auto targetBlend = getBlend01();
    const auto sourceMicIndex = getSourceMicIndex();
    const auto sourceMicChanged = sourceMicIndex != lastSourceMicIndex;

    if (sourceMicChanged)
        blendSmoother.setCurrentAndTargetValue (targetBlend);
    else
        blendSmoother.setTargetValue (targetBlend);

    auto coefficientBlend = targetBlend;

    if (blendSmoother.isSmoothing())
    {
        coefficientBlend = blendSmoother.skip (buffer.getNumSamples());
    }
    else
    {
        blendSmoother.setCurrentAndTargetValue (targetBlend);
    }

    if (coefficientBlend <= 0.0001f && targetBlend <= 0.0001f && ! sourceMicChanged)
        return;

    if (std::abs (coefficientBlend - lastFilterBlend) > 0.0001f || sourceMicChanged)
        rebuildFilters (coefficientBlend, sourceMicIndex, sourceMicChanged);

    const auto channelsToProcess = juce::jmin (buffer.getNumChannels(), static_cast<int> (filters.size()));

    for (auto channel = 0; channel < channelsToProcess; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        auto& channelFilters = filters[static_cast<size_t> (channel)];

        for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            auto sample = samples[sampleIndex];

            for (auto& filter : channelFilters)
                sample = filter.processSample (sample);

            samples[sampleIndex] = sample;
        }
    }
}

bool RodeM2ToSlateML1AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* RodeM2ToSlateML1AudioProcessor::createEditor()
{
    return new RodeM2ToSlateML1AudioProcessorEditor (*this);
}

void RodeM2ToSlateML1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RodeM2ToSlateML1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

float RodeM2ToSlateML1AudioProcessor::getBlendPercent() const
{
    return blendParameter != nullptr ? blendParameter->load() : 100.0f;
}

float RodeM2ToSlateML1AudioProcessor::getBlend01() const
{
    return EQModel::normaliseBlendPercent (getBlendPercent());
}

int RodeM2ToSlateML1AudioProcessor::getSourceMicIndex() const
{
    const auto rawIndex = sourceMicParameter != nullptr ? juce::roundToInt (sourceMicParameter->load()) : 0;
    return EQModel::getValidProfileIndex (rawIndex);
}

juce::String RodeM2ToSlateML1AudioProcessor::getSourceMicName() const
{
    return EQModel::getProfile (getSourceMicIndex()).name;
}

double RodeM2ToSlateML1AudioProcessor::getAnalysisSampleRate() const
{
    return activeSampleRate;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RodeM2ToSlateML1AudioProcessor();
}
