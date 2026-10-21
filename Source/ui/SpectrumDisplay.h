/*
  ==============================================================================

    Closer - Rave Factory
    Pantalla de espectro (Mid y Side)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

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
