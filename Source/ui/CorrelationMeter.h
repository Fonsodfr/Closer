/*
  ==============================================================================

    Closer - Rave Factory
    Medidor de correlación de fase

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

//==============================================================================
/**
    F4: medidor de correlación de fase.
    +1 = L y R iguales (mono) · 0 = sin relación · -1 = opuestos (se anulan en mono).
    En verde la zona segura, en naranja la que da problemas en mono y en vinilo.
*/
class CorrelationMeter  : public juce::Component,
                          private juce::Timer
{
public:
    explicit CorrelationMeter (ImagerAudioProcessor&);
    ~CorrelationMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ImagerAudioProcessor& audioProcessor;
    float value = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrelationMeter)
};
