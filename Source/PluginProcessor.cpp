#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

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

    for (auto& channelDynamicBands : dynamicBandStates)
        for (auto& dynamicBand : channelDynamicBands)
        {
            dynamicBand.bandpass.reset();
            dynamicBand.envelope = 0.0f;
        }
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

    if (static_cast<int> (filters.size()) == safeChannelCount
        && static_cast<int> (dynamicBandStates.size()) == safeChannelCount)
        return;

    filters.clear();
    filters.resize (static_cast<size_t> (safeChannelCount));
    dynamicBandStates.clear();
    dynamicBandStates.resize (static_cast<size_t> (safeChannelCount));

    for (auto& channelFilters : filters)
        channelFilters.resize (EQModel::bandCount);

    for (auto& channelDynamicBands : dynamicBandStates)
        channelDynamicBands.resize (EQModel::dynamicBandCount);

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

    for (auto& channelDynamicBands : dynamicBandStates)
    {
        for (size_t i = 0; i < profile.dynamicBands.size(); ++i)
        {
            auto& state = channelDynamicBands[i];
            const auto& band = profile.dynamicBands[i];
            const auto frequency = static_cast<float> (EQModel::clampFrequency (band.frequencyHz, activeSampleRate));
            const auto q = juce::jmax (0.1f, band.q);
            const auto attackSeconds = juce::jmax (0.0005f, band.attackMs * 0.001f);
            const auto releaseSeconds = juce::jmax (0.001f, band.releaseMs * 0.001f);

            if (auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (activeSampleRate, frequency, q))
                *state.bandpass.coefficients = *coefficients;

            state.attackCoefficient = std::exp (-1.0f / static_cast<float> (activeSampleRate * attackSeconds));
            state.releaseCoefficient = std::exp (-1.0f / static_cast<float> (activeSampleRate * releaseSeconds));

            if (resetState)
            {
                state.bandpass.reset();
                state.envelope = 0.0f;
            }
        }
    }

    lastFilterBlend = safeBlend;
    lastSourceMicIndex = safeSourceMicIndex;
}

void RodeM2ToSlateML1AudioProcessor::processDynamicBands (float& sample,
                                                          std::vector<DynamicBandState>& channelDynamicBands,
                                                          const EQModel::Profile& profile,
                                                          float blend01)
{
    const auto safeBlend = juce::jlimit (0.0f, 2.0f, blend01);

    if (safeBlend <= 0.0001f)
        return;

    for (size_t i = 0; i < profile.dynamicBands.size(); ++i)
    {
        const auto& band = profile.dynamicBands[i];

        if (band.maxReductionDb <= 0.0f || band.ratio <= 1.0f)
            continue;

        auto& state = channelDynamicBands[i];
        const auto bandSignal = state.bandpass.processSample (sample);
        const auto detector = std::abs (bandSignal);
        const auto coefficient = detector > state.envelope ? state.attackCoefficient : state.releaseCoefficient;

        state.envelope = coefficient * state.envelope + (1.0f - coefficient) * detector;

        const auto envelopeDb = juce::Decibels::gainToDecibels (state.envelope, -120.0f);

        if (envelopeDb <= band.thresholdDb)
            continue;

        const auto overDb = envelopeDb - band.thresholdDb;
        const auto compressedDb = overDb / band.ratio;
        const auto reductionDb = juce::jmin (band.maxReductionDb * safeBlend, overDb - compressedDb);
        const auto dynamicGain = juce::Decibels::decibelsToGain (-juce::jmax (0.0f, reductionDb));

        sample += (dynamicGain - 1.0f) * bandSignal;
    }
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
    const auto& profile = EQModel::getProfile (sourceMicIndex);

    for (auto channel = 0; channel < channelsToProcess; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);
        auto& channelFilters = filters[static_cast<size_t> (channel)];
        auto& channelDynamicBands = dynamicBandStates[static_cast<size_t> (channel)];

        for (auto sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            auto sample = samples[sampleIndex];

            for (auto& filter : channelFilters)
                sample = filter.processSample (sample);

            processDynamicBands (sample, channelDynamicBands, profile, coefficientBlend);

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
