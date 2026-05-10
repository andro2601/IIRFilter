/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <complex>
#include <vector>

using namespace juce;

//==============================================================================
/**
*/
class IIRFilterAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    IIRFilterAudioProcessor();
    ~IIRFilterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

	//==============================================================================
    void updateCoefficients(double sampleRate);
	void butterworthCoefficients(std::vector<juce::dsp::IIR::Coefficients<double>>& lpCoeffs, std::vector<juce::dsp::IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate);
    void chebyshevICoefficients(std::vector<juce::dsp::IIR::Coefficients<double>>& lpCoeffs, std::vector<juce::dsp::IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate);
    void chebyshevIICoefficients(std::vector<juce::dsp::IIR::Coefficients<double>>& lpCoeffs, std::vector<juce::dsp::IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate);
    void ellipticCoefficients(std::vector<juce::dsp::IIR::Coefficients<double>>& lpCoeffs, std::vector<juce::dsp::IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate);
	void besselCoefficients(std::vector<juce::dsp::IIR::Coefficients<double>>& lpCoeffs, std::vector<juce::dsp::IIR::Coefficients<double>>& hpCoeffs, double lpCutoff, double hpCutoff, int filterOrder, double sampleRate);

    // Helper struct for our lookup table
    struct Root {
        std::complex<double> pole;
        std::complex<double> zero;
    };

    /*struct FileLogger : public Logger
    {
        FileLogger(const File& f)
        {
            file = f;
            file.deleteFile(); // optional: start fresh
            stream = file.createOutputStream();
            if (!stream)
            {
                // handle error (e.g. log and return)
                Logger::writeToLog("Failed to open log file for writing");
            }
        }

        ~FileLogger() override
        {
            stream.reset();
        }

        void logMessage(const String& message) override
        {
            if (stream)
            {
                const String line = Time::getCurrentTime().toString(true, true)
                    + "  " + message + newLine;
                stream->writeText(line, false, false, nullptr);
                stream->flush();
            }
        }

    private:
        File file;
        std::unique_ptr<FileOutputStream> stream;
    };*/

    std::vector<IIRFilterAudioProcessor::Root> getEllipticProto(int filterOrder);
    std::vector<IIRFilterAudioProcessor::Root> getBesselProto(int filterOrder);

    AudioProcessorValueTreeState parameters;

    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::unique_ptr<juce::dsp::Oversampling<double>> oversampler;
    juce::AudioBuffer<double> doubleBuffer;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    // Filters
    using StereoIIR = dsp::ProcessorDuplicator<dsp::IIR::Filter<double>, dsp::IIR::Coefficients<double>>;

    using FilterChain = dsp::ProcessorChain<StereoIIR, StereoIIR, StereoIIR, StereoIIR>;

    FilterChain highPassChain, lowPassChain;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedLpCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedHpCutoff;

    // Stored values for knowing when to update coefficients
	float lastLpCutoffParam = -1.0f;  // Store last parameter value for LPF cutoff
	float lastHpCutoffParam = -1.0f;  // Store last parameter value for HPF cutoff
    float lastLpCutoff = -1.0f;  // Store last used LPF cutoff
    float lastHpCutoff = -1.0f;  // Store last used HPF cutoff
	int lastFilterOrder = -1; // Store last used filterOrder
	int lastApproximationType = -1; // Store last used approximation type index

	int silentBlockCount = 0; // Counter for consecutive silent blocks
    int currentOversamplingRatio = 1;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IIRFilterAudioProcessor)
};
