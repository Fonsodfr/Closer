/*
  ==============================================================================

    Closer - Rave Factory
    Medidor de correlación de fase

  ==============================================================================
*/

#include "CorrelationMeter.h"
#include "Theme.h"

using namespace theme;

//==============================================================================
// CorrelationMeter
//==============================================================================
CorrelationMeter::CorrelationMeter (ImagerAudioProcessor& p) : audioProcessor (p)
{
    startTimerHz (30);
}

CorrelationMeter::~CorrelationMeter()
{
    stopTimer();
}

void CorrelationMeter::timerCallback()
{
    const float newValue = juce::jlimit (-1.0f, 1.0f, audioProcessor.getCorrelation());
    if (std::abs (newValue - value) > 0.005f)
    {
        value = newValue;
        repaint();
    }
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Barra de 20 segmentos de -1 a +1, encendida desde el centro hacia el valor
    auto bar = area.removeFromTop (22.0f);
    g.setColour (lcdFill);
    g.fillRect (bar);
    g.setColour (lcdBorder);
    g.drawRect (bar, 1.0f);

    constexpr int numSegments = 20;
    const auto inner = bar.reduced (3.0f);
    const float segW = inner.getWidth() / (float) numSegments;
    const int   lit  = juce::roundToInt (value * 10.0f);                 // de -10 a +10 segmentos

    for (int i = 0; i < numSegments; ++i)
    {
        const int fromCentre = i - numSegments / 2;                       // -10 … +9
        const bool on = (lit >= 0) ? (fromCentre >= 0 && fromCentre < lit)
                                   : (fromCentre < 0 && fromCentre >= lit);
        juce::Colour c (0xff16221a);
        if (on)
            c = (fromCentre >= 0) ? green : warning;

        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (inner.getX() + (float) i * segW + 1.0f, inner.getY(), segW - 2.0f, inner.getHeight()));
    }

    // Escala
    area.removeFromTop (4.0f);
    auto scale = area.removeFromTop (14.0f);
    g.setFont (lcdFont (11.0f));
    g.setColour (textDim);
    g.drawText ("-1", scale, juce::Justification::centredLeft);
    g.drawText ("0",  scale, juce::Justification::centred);
    g.drawText ("+1", scale, juce::Justification::centredRight);

    // Valor numérico
    area.removeFromTop (4.0f);
    g.setFont (lcdFont (18.0f));
    g.setColour (value >= 0.0f ? green : warning);
    g.drawText ((value >= 0.0f ? "+" : "") + juce::String (value, 2), area.removeFromTop (22.0f),
                juce::Justification::centred);
}
