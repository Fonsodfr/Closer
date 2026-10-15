/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

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

//==============================================================================
/**
*/
class ImagerAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ImagerAudioProcessor();
    ~ImagerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    AnalyserFifo& getAnalyser() { return analyser; }

    // F4: el botón de bypass del DAW controla nuestro propio bypass (con fundido sin clics)
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    // F4: correlación de fase de la salida, de -1 (fuera de fase) a +1 (mono).
    // La lee el medidor de la interfaz; atómica para no bloquear el audio.
    float getCorrelation() const { return correlation.load(); }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // F2: un suavizado por banda para que mover cualquier mando no haga "zipper noise"
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedBass, smoothedMid, smoothedHigh;

    // F2: crossover de 3 bandas Linkwitz-Riley de 24 dB/oct (LR4), solo sobre la señal Side
    juce::dsp::LinkwitzRileyFilter<float> lowSplit;    // corte grave/medio  (p. ej. 200 Hz)
    juce::dsp::LinkwitzRileyFilter<float> highSplit;   // corte medio/agudo  (p. ej. 4 kHz)
    juce::dsp::LinkwitzRileyFilter<float> lowAllpass;  // compensa la fase de la banda grave en el segundo corte

    float readParam (const char* id, float fallback) const;

    AnalyserFifo analyser;   // F3: muestras para el visualizador de espectro

    // F4: bypass con fundido de 20 ms (0 = procesado, 1 = señal original)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;

    // F4: correlación de fase suavizada
    std::atomic<float> correlation { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImagerAudioProcessor)
};
