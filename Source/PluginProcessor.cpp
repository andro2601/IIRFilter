/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dsp;

//==============================================================================
IIRFilterAudioProcessor::IIRFilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

IIRFilterAudioProcessor::~IIRFilterAudioProcessor()
{
}

//==============================================================================
const String IIRFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool IIRFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool IIRFilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double IIRFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int IIRFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int IIRFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IIRFilterAudioProcessor::setCurrentProgram (int index)
{
}

const String IIRFilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void IIRFilterAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void IIRFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getMainBusNumOutputChannels();

    filter.prepare(spec);
    filter.reset();

    updateCoefficients(sampleRate);
}

void IIRFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool IIRFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void IIRFilterAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    AudioBlock<float> block(buffer);

    updateCoefficients(getSampleRate());
    filter.process(ProcessContextReplacing<float>(block));
    
}

void IIRFilterAudioProcessor::updateCoefficients(double sampleRate) {
    float lpCutoff = parameters.getRawParameterValue("lpCutoff")->load();
    float hpCutoff = parameters.getRawParameterValue("hpCutoff")->load();
	float lpQ = parameters.getRawParameterValue("lpQ")->load();
	float hpQ = parameters.getRawParameterValue("hpQ")->load();

    if (lpCutoff == lastLpCutoff && hpCutoff == lastHpCutoff && lpQ == lastLpQ && hpQ == lastHpQ) return;

    lastLpCutoff = lpCutoff;
    lastHpCutoff = hpCutoff;
	lastLpQ = lpQ;
	lastHpQ = hpQ;

    // 1. Pre-calculate intermediate variables
    double LPomega = 2.0 * juce::MathConstants<double>::pi * lpCutoff / sampleRate;
    double LPsin = std::sin(LPomega);
    double LPcos = std::cos(LPomega);
    double LPalpha = LPsin / (2.0 * lpQ);

	double HPomega = 2.0 * juce::MathConstants<double>::pi * hpCutoff / sampleRate;
	double HPsin = std::sin(HPomega);
	double HPcos = std::cos(HPomega);
	double HPalpha = HPsin / (2.0 * hpQ);

    // 2. Calculate raw coefficients
    double LPb0 = (1.0 - LPcos) / 2.0;
    double LPb1 = 1.0 - LPcos;
    double LPb2 = (1.0 - LPcos) / 2.0;
    double LPa0 = 1.0 + LPalpha;
    double LPa1 = -2.0 * LPcos;
    double LPa2 = 1.0 - LPalpha;

	double HPb0 = (1.0 + HPcos) / 2.0;
	double HPb1 = -(1.0 + HPcos);
	double HPb2 = (1.0 + HPcos) / 2.0;
	double HPa0 = 1.0 + HPalpha;
	double HPa1 = -2.0 * HPcos;
    double HPa2 = 1.0 - HPalpha;

    // 3. Normalize and set to JUCE filter
    // We divide everything by a0 so that the feedback leading coefficient is 1.0
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>(
        static_cast<float>(LPb0 / LPa0),
        static_cast<float>(LPb1 / LPa0),
        static_cast<float>(LPb2 / LPa0),
        1.0f, // a0 becomes 1
        static_cast<float>(LPa1 / LPa0),
        static_cast<float>(LPa2 / LPa0)
    );

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>(
        static_cast<float>(HPb0 / HPa0),
        static_cast<float>(HPb1 / HPa0),
        static_cast<float>(HPb2 / HPa0),
        1.0f, // a0 becomes 1
        static_cast<float>(HPa1 / HPa0),
        static_cast<float>(HPa2 / HPa0)
	);

    *filter.get<0>().state = lpCoeffs;
    *filter.get<1>().state = hpCoeffs;
}

AudioProcessorValueTreeState::ParameterLayout IIRFilterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<AudioParameterFloat>("lpCutoff", "Low Pass Cutoff Frequency", NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 20000.f, AudioParameterFloatAttributes()));
    parameters.push_back(std::make_unique<AudioParameterFloat>("hpCutoff", "High Pass Cutoff Frequency", NormalisableRange<float>(10.f, 20000.f, 1.f, 0.5f, false), 10.f, AudioParameterFloatAttributes()));
	parameters.push_back(std::make_unique<AudioParameterFloat>("lpQ", "Low Pass Q Factor", NormalisableRange<float>(0.1f, 10.f, 0.1f), 0.7071f, AudioParameterFloatAttributes()));
	parameters.push_back(std::make_unique<AudioParameterFloat>("hpQ", "High Pass Q Factor", NormalisableRange<float>(0.1f, 10.f, 0.1f), 0.7071f, AudioParameterFloatAttributes()));

    return { parameters.begin(), parameters.end() };
}

//==============================================================================
bool IIRFilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* IIRFilterAudioProcessor::createEditor()
{
    return new IIRFilterAudioProcessorEditor (*this);
}

//==============================================================================
void IIRFilterAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void IIRFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IIRFilterAudioProcessor();
}
