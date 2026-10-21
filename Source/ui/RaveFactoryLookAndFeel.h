/*
  ==============================================================================

    Closer - Rave Factory
    Estilo de los mandos y botones

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

//==============================================================================
/**
    F4: el "estilo" Rave Factory. En JUCE, un LookAndFeel decide cómo se dibujan
    los controles estándar (mandos, botones, cajas de texto). Cambiándolo, todos
    los mandos del plugin pasan a tener el aspecto del diseño sin tocar su lógica.
*/
class RaveFactoryLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    RaveFactoryLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
