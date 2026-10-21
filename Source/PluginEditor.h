/*
  ==============================================================================

    Closer - Rave Factory
    Ventana del plugin: coloca las piezas de la carpeta ui y dibuja el chasis

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/SpectrumDisplay.h"
#include "ui/RaveFactoryLookAndFeel.h"
#include "ui/CorrelationMeter.h"

//==============================================================================
/**
    F4: interfaz Rave Factory: chasis cepillado, cabecera con el logo RF,
    pantalla de espectro, mandos por banda, cortes, medidor de fase y bypass.
*/
class ImagerAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ImagerAudioProcessorEditor (ImagerAudioProcessor&);
    ~ImagerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ImagerAudioProcessor& audioProcessor;

    // El LookAndFeel va PRIMERO: así se destruye el último, cuando ya no hay
    // ningún mando que lo esté usando.
    RaveFactoryLookAndFeel lookAndFeel;

    SpectrumDisplay  spectrum;
    CorrelationMeter phaseMeter;

    // Un "mando" = rueda + etiqueta + enlace con su parámetro
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Orden: LOW, MIDS, HIGH (anchura) y los dos cortes de frecuencia
    std::array<Knob, 5> knobs;

    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Dibuja todo lo que no se mueve: chasis, cabecera, paneles y textos fijos
    void drawBackground (juce::Graphics&);

    void setupKnob (Knob& knob, const juce::String& paramID, const juce::String& title, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImagerAudioProcessorEditor)
};
