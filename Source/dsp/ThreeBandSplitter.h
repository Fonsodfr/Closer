/*
  ==============================================================================

    Closer - Rave Factory
    Crossover de 3 bandas para la señal Side

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Parte una señal (el Side) en graves, medios y agudos con dos cortes
    Linkwitz-Riley de 24 dB/oct (LR4). Al volver a sumar las tres bandas sale
    la señal original (con un giro de fase plano), sin huecos ni bultos.

      corte 1 (lowSplit):  graves | resto
      corte 2 (highSplit): resto -> medios | agudos
      lowAllpass: da a los graves el mismo giro de fase que el corte 2 da a
                  medios y agudos, para que las tres bandas sumen bien.
*/
class ThreeBandSplitter
{
public:
    void prepare (double sampleRate, int samplesPerBlock)
    {
        // Solo filtra un canal (el Side), por eso numChannels = 1
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };

        lowSplit  .prepare (spec);
        highSplit .prepare (spec);
        lowAllpass.prepare (spec);
        lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    }

    void setCrossovers (float lowHz, float highHz)
    {
        lowSplit  .setCutoffFrequency (lowHz);
        highSplit .setCutoffFrequency (highHz);
        lowAllpass.setCutoffFrequency (highHz);   // siempre en la misma frecuencia que el segundo corte
    }

    void reset()
    {
        lowSplit.reset();
        highSplit.reset();
        lowAllpass.reset();
    }

    void process (float input, float& low, float& mid, float& high) noexcept
    {
        float rest;
        lowSplit .processSample (0, input, low, rest);   // graves | resto
        highSplit.processSample (0, rest,  mid, high);   // medios | agudos
        low = lowAllpass.processSample (0, low);
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> lowSplit;    // corte grave/medio  (p. ej. 200 Hz)
    juce::dsp::LinkwitzRileyFilter<float> highSplit;   // corte medio/agudo  (p. ej. 4 kHz)
    juce::dsp::LinkwitzRileyFilter<float> lowAllpass;  // compensa la fase de la banda grave en el segundo corte
};
