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
    // High-Pass Filter Slider
    hpCutoffSlider.setSliderStyle(Slider::Rotary);
    hpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible(hpCutoffSlider);

    hpCutoffLabel.setText("High-Pass Cutoff", dontSendNotification);
    //hpCutoffLabel.attachToComponent(&hpCutoffSlider, false);
    hpCutoffLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(hpCutoffLabel);

    // Low-Pass Filter Slider
    lpCutoffSlider.setSliderStyle(Slider::Rotary);
    lpCutoffSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible(lpCutoffSlider);

    lpCutoffLabel.setText("Low-Pass Cutoff", dontSendNotification);
    //lpCutoffLabel.attachToComponent(&lpCutoffSlider, false);
    lpCutoffLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(lpCutoffLabel);


	// filterOrderSlider
    filterOrderSlider.setSliderStyle(Slider::Rotary);
    filterOrderSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
	addAndMakeVisible(filterOrderSlider);

	filterOrderLabel.setText("Filter Order", dontSendNotification);
	filterOrderLabel.setJustificationType(Justification::centred);
	addAndMakeVisible(filterOrderLabel);

    // approximationMenu
	addAndMakeVisible(approximationMenu);

    // Bypass buttons
    addAndMakeVisible(bypassHpButton);
    bypassHpButton.setButtonText("Bypass HP");
    addAndMakeVisible(bypassLpButton);
    bypassLpButton.setButtonText("Bypass LP");

    // Attach sliders to parameters
    lpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "lpCutoff", lpCutoffSlider);

    hpCutoffAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "hpCutoff", hpCutoffSlider);

    filterOrderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
		audioProcessor.parameters, "filterOrder", filterOrderSlider);

	approximationMenu.addItemList(audioProcessor.parameters.getParameter("approximation")->getAllValueStrings(), 1);
    approximationAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
		audioProcessor.parameters, "approximation", approximationMenu);

    bypassHpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bypassHp", bypassHpButton);

    bypassLpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bypassLp", bypassLpButton);

    getLookAndFeel().setColour(Slider::thumbColourId, Colours::cyan);
    getLookAndFeel().setColour(Slider::rotarySliderOutlineColourId, Colour(0xFF2d2d2d));

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(450, 400);
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
    auto area = getLocalBounds().reduced(20);

    // Header/Menu Area
    auto topRow = area.removeFromTop(40);
    approximationMenu.setBounds(topRow.withSizeKeepingCentre(200, 30));

    area.removeFromTop(10); // Spacer

    // --- MAIN SLIDERS (Large) ---
    auto mainSliderArea = area.removeFromTop(200);
    auto hpArea = mainSliderArea.removeFromLeft(mainSliderArea.getWidth() / 2).reduced(5, 0);
    auto lpArea = mainSliderArea.reduced(5, 0);

    // HP Layout
    auto hpHeader = hpArea.removeFromTop(25);
    hpCutoffLabel.setBounds(hpHeader);
    bypassHpButton.setBounds(hpHeader.removeFromLeft(30)); // Small height, centered horizontal padding
    hpCutoffSlider.setBounds(hpArea);

    // LP Layout
    auto lpHeader = lpArea.removeFromTop(25);
    lpCutoffLabel.setBounds(lpHeader);
    bypassLpButton.setBounds(lpHeader.removeFromLeft(30)); // Small height, centered horizontal padding
    lpCutoffSlider.setBounds(lpArea);

    area.removeFromTop(30); // Spacer

    // --- SECONDARY CONTROLS (Small) ---
    // We center the smaller order slider at the bottom
    auto bottomArea = area.removeFromTop(100);
    auto orderArea = bottomArea.withSizeKeepingCentre(80, 100); // Narrower width = "Smaller" look

    filterOrderLabel.setBounds(orderArea.removeFromTop(20));
    filterOrderSlider.setBounds(orderArea);
}
