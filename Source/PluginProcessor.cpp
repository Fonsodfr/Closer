/*
  ==============================================================================

    Closer - Rave Factory
    Procesado de audio: matriz Mid/Side, anchura por banda, bypass y correlación

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ImagerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // F2: tres mandos de anchura, uno por banda (0% = mono, 100% = estéreo original)
    auto addWidth = [&params] (const char* id, const char* name)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },
            name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")
                                                 .withStringFromValueFunction ([] (float value, int) {
                                                     return juce::String (value, 1) + " %";
                                                 })));
    };

    addWidth ("bassWidth", "Low Width");
    addWidth ("midWidth",  "Mids Width");
    addWidth ("highWidth", "High Width");

    // F2: dos frecuencias de corte. El último número del rango (skew) hace que el mando
    // dedique más recorrido a las frecuencias bajas, como un EQ de verdad.
    auto hzText = juce::AudioParameterFloatAttributes().withLabel ("Hz")
                      .withStringFromValueFunction ([] (float value, int) {
                          return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                                  : juce::String ((int) value) + " Hz";
                      });

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lowXover", 1 }, "Low Crossover",
        juce::NormalisableRange<float> (40.0f, 500.0f, 1.0f, 0.5f), 200.0f, hzText));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "highXover", 1 }, "High Crossover",
        juce::NormalisableRange<float> (1000.0f, 12000.0f, 10.0f, 0.5f), 4000.0f, hzText));

    // F4: bypass (también lo usa el botón de bypass del propio DAW)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

// Lee un parámetro de forma segura para el hilo de audio (lectura atómica, sin bloqueos)
float ImagerAudioProcessor::readParam (const char* id, float fallback) const
{
    auto* raw = apvts.getRawParameterValue (id);
    return raw != nullptr ? raw->load() : fallback;
}

//==============================================================================
ImagerAudioProcessor::ImagerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

ImagerAudioProcessor::~ImagerAudioProcessor()
{
}

//==============================================================================
const juce::String ImagerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ImagerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ImagerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ImagerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ImagerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ImagerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ImagerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ImagerAudioProcessor::setCurrentProgram (int)
{
}

const juce::String ImagerAudioProcessor::getProgramName (int)
{
    return {};
}

void ImagerAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void ImagerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Rampa de 20 ms en cada banda para eliminar el zipper noise al mover los mandos
    smoothedBass.reset (sampleRate, 0.02);
    smoothedMid .reset (sampleRate, 0.02);
    smoothedHigh.reset (sampleRate, 0.02);
    smoothedBass.setCurrentAndTargetValue (readParam ("bassWidth", 100.0f) / 100.0f);
    smoothedMid .setCurrentAndTargetValue (readParam ("midWidth",  100.0f) / 100.0f);
    smoothedHigh.setCurrentAndTargetValue (readParam ("highWidth", 100.0f) / 100.0f);

    bypassMix.reset (sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (readParam ("bypass", 0.0f) > 0.5f ? 1.0f : 0.0f);
    correlation.store (1.0f);

    // Los filtros necesitan saber la frecuencia de muestreo
    sideBands.prepare (sampleRate, samplesPerBlock);
    sideBands.setCrossovers (readParam ("lowXover", 200.0f), readParam ("highXover", 4000.0f));
    sideBands.reset();
}

void ImagerAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ImagerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ImagerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 1. Leer los mandos una vez por bloque (Real-Time Safe: sin bloqueos ni asignaciones)
    smoothedBass.setTargetValue (readParam ("bassWidth", 100.0f) / 100.0f);
    smoothedMid .setTargetValue (readParam ("midWidth",  100.0f) / 100.0f);
    smoothedHigh.setTargetValue (readParam ("highWidth", 100.0f) / 100.0f);

    const float lowHz  = readParam ("lowXover",  200.0f);
    const float highHz = readParam ("highXover", 4000.0f);
    sideBands.setCrossovers (lowHz, highHz);

    bypassMix.setTargetValue (readParam ("bypass", 0.0f) > 0.5f ? 1.0f : 0.0f);

    // El procesamiento Mid/Side requiere canales estéreo
    if (totalNumInputChannels >= 2 && totalNumOutputChannels >= 2)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        const int numSamples = buffer.getNumSamples();

        // F3: reservar hueco en el buzón del visualizador. El buzón es circular,
        // así que el hueco puede venir en dos trozos (final y principio del buffer).
        int start1, size1, start2, size2;
        analyser.fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        // F4: sumas para la correlación de fase de este bloque
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float l = left[i];
            const float r = right[i];

            // 2. Matriz Mid/Side. El Mid NO se toca nunca: la suma mono queda intacta.
            const float mid  = 0.5f * (l + r);
            const float side = 0.5f * (l - r);

            // 3. Partir el Side en tres bandas (graves, medios, agudos)
            float sideLow, sideMid, sideHigh;
            sideBands.process (side, sideLow, sideMid, sideHigh);

            // 4. Aplicar la anchura de cada banda y volver a sumar el Side
            const float sideOut = sideLow  * smoothedBass.getNextValue()
                                + sideMid  * smoothedMid .getNextValue()
                                + sideHigh * smoothedHigh.getNextValue();

            // 5. Reconstrucción a Left y Right, con fundido hacia la señal original si hay bypass
            const float dry = bypassMix.getNextValue();
            const float outL = (mid + sideOut) * (1.0f - dry) + l * dry;
            const float outR = (mid - sideOut) * (1.0f - dry) + r * dry;
            left[i]  = outL;
            right[i] = outR;

            sumLR += (double) outL * outR;
            sumLL += (double) outL * outL;
            sumRR += (double) outR * outR;

            // 6. F3: copiar Mid y Side (ya procesado) al buzón del visualizador
            int slot = -1;
            if (i < size1)                slot = start1 + i;
            else if (i - size1 < size2)   slot = start2 + (i - size1);

            if (slot >= 0)
            {
                analyser.mid [(size_t) slot] = mid;
                analyser.side[(size_t) slot] = 0.5f * (outL - outR);   // el Side que sale de verdad (respeta el bypass)
            }
        }

        analyser.fifo.finishedWrite (size1 + size2);

        // F4: correlación = parecido entre L y R. +1 = iguales (mono), 0 = sin relación,
        // -1 = opuestos (se anulan en mono). Si hay silencio no se actualiza.
        const double energy = std::sqrt (sumLL * sumRR);
        if (energy > 1.0e-9)
        {
            const float blockCorr = (float) (sumLR / energy);
            correlation.store (0.85f * correlation.load() + 0.15f * blockCorr);   // suavizado
        }
    }
}

//==============================================================================
bool ImagerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ImagerAudioProcessor::createEditor()
{
    return new ImagerAudioProcessorEditor (*this);
}

//==============================================================================
void ImagerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ImagerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ImagerAudioProcessor();
}
