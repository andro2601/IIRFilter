/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
IIRFilterAudioProcessorEditor::IIRFilterAudioProcessorEditor (IIRFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Low-Pass Filter Slider
    lpCutoffSlider.setSliderStyle(Slider::Rotary);
    lpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(lpCutoffSlider);

    lpLabel.setText("Low-Pass Cutoff", dontSendNotification);
    lpLabel.attachToComponent(&lpCutoffSlider, false);
	lpLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(lpLabel);

    // High-Pass Filter Slider
    hpCutoffSlider.setSliderStyle(Slider::Rotary);
    hpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(hpCutoffSlider);

    hpLabel.setText("High-Pass Cutoff", dontSendNotification);
    hpLabel.attachToComponent(&hpCutoffSlider, false);
    hpLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(hpLabel);

	// Low-Pass Q Slider
	lpQSlider.setSliderStyle(Slider::Rotary);
	lpQSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
	addAndMakeVisible(lpQSlider);

	lpQLabel.setText("Low-Pass Q", dontSendNotification);
	lpQLabel.attachToComponent(&lpQSlider, false);
	lpQLabel.setJustificationType(juce::Justification::centredBottom);
	addAndMakeVisible(lpQLabel);

	// High-Pass Q Slider
	hpQSlider.setSliderStyle(Slider::Rotary);
	hpQSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);
	addAndMakeVisible(hpQSlider);

	hpQLabel.setText("High-Pass Q", dontSendNotification);
	hpQLabel.attachToComponent(&hpQSlider, false);
    hpQLabel.setJustificationType(juce::Justification::centredBottom);
	addAndMakeVisible(hpQLabel);

    // Attach sliders to parameters
    lpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "lpCutoff", lpCutoffSlider);

    hpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "hpCutoff", hpCutoffSlider);

    lpQAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.parameters, "lpQ", lpQSlider);

	hpQAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.parameters, "hpQ", hpQSlider);

    getLookAndFeel().setColour(Slider::thumbColourId, Colours::cyan);
    getLookAndFeel().setColour(Slider::rotarySliderOutlineColourId, Colour(0xFF2d2d2d));

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(400, 300);
}

IIRFilterAudioProcessorEditor::~IIRFilterAudioProcessorEditor()
{
}

//==============================================================================
void IIRFilterAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    g.setColour (Colours::white);
    g.setFont (FontOptions (15.0f));
    g.drawFittedText ("IIR Filter", getLocalBounds(), Justification::centredTop, 1);
}

void IIRFilterAudioProcessorEditor::resized()
{
    // Layout positions
    int sliderWidth = 150;
    int sliderHeight = 150;

    auto bounds = getLocalBounds().reduced(10);
    auto hpfArea = bounds.removeFromLeft(bounds.getWidth() / 2).reduced(10);
    auto lpfArea = bounds.reduced(10); // The remaining right side

    // --- Helper function logic for HPF Area ---
    hpLabel.setBounds(hpfArea.removeFromTop(20));      // Title at the top

    auto hpQArea = hpfArea.removeFromBottom(90);         // Reserve space at bottom for Q
    hpCutoffSlider.setBounds(hpfArea);                   // Freq takes all remaining middle space

    hpQLabel.setBounds(hpQArea.removeFromTop(30));       // Q Label 
    hpQSlider.setBounds(hpQArea);                        // Q Slider stays in the bottom box

    // --- Repeat similar logic for LPF Area ---
	lpLabel.setBounds(lpfArea.removeFromTop(20));      // Title at the top

    auto lpQArea = lpfArea.removeFromBottom(90);      // Reserve space at bottom for Q
    lpCutoffSlider.setBounds(lpfArea);                 // Freq takes all remaining middle space
    lpQLabel.setBounds(lpQArea.removeFromTop(30));    // Q Label 
	lpQSlider.setBounds(lpQArea);                    // Q Slider stays in the bottom box
}
