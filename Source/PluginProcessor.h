/*
  ==============================================================================

    Closer - Rave Factory
    Imagen estéreo en 3 bandas: el Mid no se toca; el Side se parte en graves,
    medios y agudos (dsp/ThreeBandSplitter.h) y cada banda tiene su anchura.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "dsp/AnalyserFifo.h"
#include "dsp/ThreeBandSplitter.h"



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
    ThreeBandSplitter sideBands;

    float readParam (const char* id, float fallback) const;

    AnalyserFifo analyser;   // F3: muestras para el visualizador de espectro

    // F4: bypass con fundido de 20 ms (0 = procesado, 1 = señal original)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;

    // F4: correlación de fase suavizada
    std::atomic<float> correlation { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImagerAudioProcessor)
};
