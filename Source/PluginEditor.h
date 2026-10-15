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
    F3: pantalla de espectro.
    - Mid  = línea verde clara  (lo que suena en el centro / en mono)
    - Side = relleno verde oscuro (lo que hay en los lados)
    - Líneas lima = las dos frecuencias de corte
*/
class SpectrumDisplay  : public juce::Component,
                         private juce::Timer
{
public:
    explicit SpectrumDisplay (ImagerAudioProcessor&);
    ~SpectrumDisplay() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void pullSamplesFromProcessor();
    void analyse (const std::vector<float>& history, std::vector<float>& smoothedDb);

    float xForFrequency (float hz) const;
    float yForDb (float db) const;
    juce::Path makeCurve (const std::vector<float>& db, bool closedForFill) const;

    ImagerAudioProcessor& audioProcessor;

    // FFT de 8192 puntos: una "barra" cada ~5 Hz, suficiente detalle para leer bien
    // los graves (antes 2048 = cada ~21 Hz, y por debajo de 100 Hz salían tramos rectos)
    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> midHistory, sideHistory;   // últimas 2048 muestras de cada señal
    std::vector<float> fftData;                   // espacio de trabajo de la FFT
    std::vector<float> midDb, sideDb;             // nivel suavizado de cada banda de frecuencia

    double sampleRate = 44100.0;

    static constexpr float minDb = -84.0f;
    static constexpr float maxDb =   0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};


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
