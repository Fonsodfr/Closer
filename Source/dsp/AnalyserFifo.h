/*
  ==============================================================================

    Closer - Rave Factory
    Buzón entre el audio y el visualizador

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    F3: "buzón" entre el hilo de audio y la pantalla.
    El audio deja aquí muestras de Mid y Side; el visualizador las recoge ~30 veces
    por segundo. AbstractFifo funciona sin bloqueos (lock-free), así que el audio
    nunca tiene que esperar a la interfaz. Si nadie lee (ventana cerrada), el
    buzón se llena y las muestras nuevas simplemente se descartan.
*/
struct AnalyserFifo
{
    static constexpr int capacity = 16384;

    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> mid  {};
    std::array<float, capacity> side {};
};
