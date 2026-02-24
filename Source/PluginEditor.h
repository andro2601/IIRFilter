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

	// Parameter controls
    Slider hpCutoffSlider;
    Slider lpCutoffSlider;
    Slider filterOrderSlider;
    ComboBox approximationMenu;
    juce::ToggleButton bypassHpButton;
    juce::ToggleButton bypassLpButton;

    // Labels
    Label hpCutoffLabel;
    Label lpCutoffLabel;
	Label filterOrderLabel;

    // Attachments to sync GUI with parameters
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hpCutoffAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> lpCutoffAttachment;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> filterOrderAttachment;
	std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> approximationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassHpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassLpAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IIRFilterAudioProcessorEditor)
};
