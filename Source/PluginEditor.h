/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class IIRFilterAudioProcessorEditor  : public AudioProcessorEditor
{
public:
    IIRFilterAudioProcessorEditor (IIRFilterAudioProcessor&);
    ~IIRFilterAudioProcessorEditor() override;

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    IIRFilterAudioProcessor& audioProcessor;

    // Sliders
    Slider lpCutoffSlider;
    Slider hpCutoffSlider;
	Slider lpQSlider;
	Slider hpQSlider;

    // Labels
    Label lpLabel;
    Label hpLabel;
	Label lpQLabel;
	Label hpQLabel;

    // Attachments to sync GUI with parameters
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> lpCutoffAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hpCutoffAttachment;
	std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> lpQAttachment;
	std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hpQAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IIRFilterAudioProcessorEditor)
};
