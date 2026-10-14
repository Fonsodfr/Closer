/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour lime      (0xffe4f22a);   // amarillo lima Rave Factory
    const juce::Colour green     (0xff8ee36b);   // verde fósforo (Mid)
    const juce::Colour darkGreen (0xff3f7d3a);   // verde oscuro (Side)
    const juce::Colour screen    (0xff0a140a);   // fondo de la pantalla
    const juce::Colour panel     (0xff1a1b1e);
    const juce::Colour header    (0xff111214);
    const juce::Colour line      (0xff2c2e33);

    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;
}

//==============================================================================
// SpectrumDisplay
//==============================================================================
SpectrumDisplay::SpectrumDisplay (ImagerAudioProcessor& p)
    : audioProcessor (p),
      midHistory  ((size_t) fftSize, 0.0f),
      sideHistory ((size_t) fftSize, 0.0f),
      fftData     ((size_t) fftSize * 2, 0.0f),
      midDb       ((size_t) fftSize / 2, minDb),
      sideDb      ((size_t) fftSize / 2, minDb)
{
    // Vaciar lo que se acumuló en el buzón mientras la ventana estaba cerrada
    auto& a = audioProcessor.getAnalyser();
    a.fifo.finishedRead (a.fifo.getNumReady());

    startTimerHz (30);   // 30 fotogramas por segundo
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::timerCallback()
{
    if (audioProcessor.getSampleRate() > 0.0)
        sampleRate = audioProcessor.getSampleRate();

    pullSamplesFromProcessor();
    analyse (midHistory,  midDb);
    analyse (sideHistory, sideDb);
    repaint();
}

// Recoge del buzón las muestras nuevas y las añade al final del historial,
// descartando las más antiguas (ventana deslizante de 2048 muestras).
void SpectrumDisplay::pullSamplesFromProcessor()
{
    auto& a = audioProcessor.getAnalyser();
    const int ready = a.fifo.getNumReady();
    if (ready <= 0)
        return;

    int start1, size1, start2, size2;
    a.fifo.prepareToRead (ready, start1, size1, start2, size2);

    auto append = [] (std::vector<float>& history, const float* source, int count)
    {
        const int n = (int) history.size();
        if (count >= n)
        {
            std::copy (source + (count - n), source + count, history.begin());
        }
        else if (count > 0)
        {
            std::move (history.begin() + count, history.end(), history.begin());
            std::copy (source, source + count, history.end() - count);
        }
    };

    append (midHistory,  a.mid.data()  + start1, size1);
    append (sideHistory, a.side.data() + start1, size1);
    append (midHistory,  a.mid.data()  + start2, size2);
    append (sideHistory, a.side.data() + start2, size2);

    a.fifo.finishedRead (size1 + size2);
}

void SpectrumDisplay::analyse (const std::vector<float>& history, std::vector<float>& smoothedDb)
{
    // 1. Copiar las muestras y suavizar los bordes con una ventana de Hann
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (history.begin(), history.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);

    // 2. FFT: convierte el tiempo en frecuencias (una "barra" cada sampleRate/2048 Hz)
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const int numBins = fftSize / 2;
    for (int k = 1; k < numBins; ++k)
    {
        // 3. Pasar a dB. Dividir por fftSize/4 hace que un seno a 0 dBFS marque 0 dB.
        const float magnitude = fftData[(size_t) k] / (fftSize * 0.25f);
        float db = juce::Decibels::gainToDecibels (magnitude, minDb - 20.0f);

        // 4. Inclinación de +4.5 dB por octava (referencia 1 kHz), como los analizadores
        //    de masterización: la música se ve más "plana" y se leen mejor los agudos.
        const float hz = (float) (k * sampleRate / fftSize);
        db += 4.5f * std::log2 (hz / 1000.0f);

        // 5. Sube rápido y baja despacio: más fácil de leer que el valor crudo
        auto& s = smoothedDb[(size_t) k];
        s = (db > s) ? db : juce::jmax (db, s - 1.2f);
    }
}

float SpectrumDisplay::xForFrequency (float hz) const
{
    // Escala logarítmica: cada octava ocupa el mismo ancho, como en un EQ
    const float proportion = std::log (hz / minHz) / std::log (maxHz / minHz);
    return proportion * (float) getWidth();
}

float SpectrumDisplay::yForDb (float db) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, (float) getHeight(), 0.0f);
}

juce::Path SpectrumDisplay::makeCurve (const std::vector<float>& db, bool closedForFill) const
{
    juce::Path path;
    const float bottom = (float) getHeight();
    bool started = false;
    float lastX = 0.0f;

    auto addPoint = [&] (float x, float y)
    {
        if (! started)
        {
            if (closedForFill) { path.startNewSubPath (x, bottom); path.lineTo (x, y); }
            else               { path.startNewSubPath (x, y); }
            started = true;
        }
        else
        {
            path.lineTo (x, y);
        }
        lastX = x;
    };

    // En los agudos caen muchas barras en el mismo píxel: nos quedamos con la más
    // alta de cada píxel. Así la curva es igual de fiel pero se dibuja mucho más rápido.
    int   currentPixel = -1;
    float pixelX = 0.0f, pixelMaxDb = minDb;

    for (int k = 1; k < (int) db.size(); ++k)
    {
        const float hz = (float) (k * sampleRate / fftSize);
        if (hz < minHz) continue;
        if (hz > maxHz) break;

        const float x = xForFrequency (hz);
        const int pixel = (int) x;

        if (pixel != currentPixel)
        {
            if (currentPixel >= 0)
                addPoint (pixelX, yForDb (pixelMaxDb));

            currentPixel = pixel;
            pixelX       = x;
            pixelMaxDb   = db[(size_t) k];
        }
        else
        {
            pixelMaxDb = juce::jmax (pixelMaxDb, db[(size_t) k]);
        }
    }

    if (currentPixel >= 0)
        addPoint (pixelX, yForDb (pixelMaxDb));

    if (started && closedForFill)
    {
        path.lineTo (lastX, bottom);
        path.closeSubPath();
    }

    return path;
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();

    // Fondo de pantalla verde oscuro
    g.setColour (screen);
    g.fillRoundedRectangle (area, 4.0f);

    // Rejilla de dB (cada 12 dB)
    g.setFont (juce::FontOptions (10.0f));
    for (float db = maxDb - 12.0f; db > minDb; db -= 12.0f)
    {
        const float y = yForDb (db);
        g.setColour (green.withAlpha (0.08f));
        g.drawHorizontalLine ((int) y, 0.0f, w);
    }

    // Rejilla de frecuencias con etiquetas
    const float freqs[]  = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i)
    {
        const float x = xForFrequency (freqs[i]);
        g.setColour (green.withAlpha (0.08f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (green.withAlpha (0.45f));
        g.drawText (labels[i], juce::Rectangle<float> (x + 3.0f, h - 14.0f, 30.0f, 12.0f),
                    juce::Justification::centredLeft);
    }

    // Banda de graves ligeramente resaltada y líneas de corte en lima
    auto& apvts = audioProcessor.getAPVTS();
    const float lowX  = xForFrequency (apvts.getRawParameterValue ("lowXover")->load());
    const float highX = xForFrequency (apvts.getRawParameterValue ("highXover")->load());

    g.setColour (lime.withAlpha (0.04f));
    g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, lowX, h));

    // Curvas: Side relleno por detrás, Mid como línea por delante
    g.setColour (darkGreen.withAlpha (0.75f));
    g.fillPath (makeCurve (sideDb, true));

    g.setColour (green);
    g.strokePath (makeCurve (midDb, false), juce::PathStrokeType (1.5f));

    for (float x : { lowX, highX })
    {
        g.setColour (lime.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (x - 2.0f, 0.0f, 5.0f, h));
        g.setColour (lime);
        g.drawVerticalLine ((int) x, 0.0f, h);
    }

    // Leyenda
    // Leyenda: cuadradito del color de cada curva + texto claro (no son botones)
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    const juce::Colour legendText (0xffb8bcc2);

    g.setColour (green);
    g.fillRect (juce::Rectangle<float> (w - 168.0f, 10.0f, 14.0f, 3.0f));            // línea = Mid
    g.setColour (legendText);
    g.drawText ("MID (CENTER)", juce::Rectangle<float> (w - 150.0f, 5.0f, 80.0f, 14.0f), juce::Justification::centredLeft);

    g.setColour (darkGreen.withAlpha (0.9f));
    g.fillRect (juce::Rectangle<float> (w - 66.0f, 6.0f, 12.0f, 10.0f));             // relleno = Side
    g.setColour (legendText);
    g.drawText ("SIDE", juce::Rectangle<float> (w - 50.0f, 5.0f, 40.0f, 14.0f), juce::Justification::centredLeft);

    g.setColour (green.withAlpha (0.6f));
    g.drawText ("SPECTRUM  +4.5 dB/OCT", juce::Rectangle<float> (8.0f, 6.0f, 200.0f, 14.0f),
                juce::Justification::centredLeft);

    // Marco
    g.setColour (juce::Colour (0xff2a2f24));
    g.drawRoundedRectangle (area.reduced (1.0f), 4.0f, 2.0f);
}

//==============================================================================
// Editor
//==============================================================================
ImagerAudioProcessorEditor::ImagerAudioProcessorEditor (ImagerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), spectrum (p)
{
    addAndMakeVisible (spectrum);

    // "MIDS" (frecuencias medias) para no confundirlo con la señal Mid (el centro)
    setupKnob (knobs[0], "bassWidth", "LOW",         lime);
    setupKnob (knobs[1], "midWidth",  "MIDS",        lime);
    setupKnob (knobs[2], "highWidth", "HIGH",        lime);
    setupKnob (knobs[3], "lowXover",  "LOW X-OVER",  green);
    setupKnob (knobs[4], "highXover", "HIGH X-OVER", green);

    setSize (640, 670);
}

ImagerAudioProcessorEditor::~ImagerAudioProcessorEditor()
{
}

void ImagerAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& paramID,
                                            const juce::String& title, juce::Colour colour)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId,    colour);
    knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, line);
    knob.slider.setColour (juce::Slider::thumbColourId,               colour);
    knob.slider.setColour (juce::Slider::textBoxTextColourId,         green);
    knob.slider.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0xff0c150b));
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0xff22301e));
    addAndMakeVisible (knob.slider);

    // Enlace bidireccional entre el mando y el parámetro del DSP
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), paramID, knob.slider);

    knob.label.setText (title, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    knob.label.setColour (juce::Label::textColourId, juce::Colour (0xffd9dadc));
    addAndMakeVisible (knob.label);
}

//==============================================================================
void ImagerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    auto bounds = getLocalBounds();

    // Cabecera
    auto headerArea = bounds.removeFromTop (70);
    g.setColour (header);
    g.fillRect (headerArea);

    g.setColour (juce::Colour (0xffecece6));
    g.setFont (juce::FontOptions (26.0f).withStyle ("Bold"));
    g.drawFittedText ("CLOSER", headerArea.removeFromTop (42), juce::Justification::centred, 1);

    g.setColour (lime);
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawFittedText ("RAVE FACTORY  /  3-BAND STEREO IMAGER", headerArea, juce::Justification::centred, 1);

    g.setColour (line);
    g.drawHorizontalLine (70, 0.0f, (float) getWidth());
    g.drawHorizontalLine (separatorY, 20.0f, (float) getWidth() - 20.0f);
    g.drawRect (getLocalBounds(), 1);
}

void ImagerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (84);

    // Pantalla de espectro
    spectrum.setBounds (bounds.removeFromTop (230).reduced (16, 0));
    bounds.removeFromTop (16);

    auto placeRow = [] (juce::Rectangle<int> row, Knob* const* rowKnobs, int count)
    {
        const int w = row.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            auto cell = row.removeFromLeft (w).reduced (12, 0);
            rowKnobs[i]->label .setBounds (cell.removeFromBottom (22));
            rowKnobs[i]->slider.setBounds (cell);
        }
    };

    // Fila 1: anchura de cada banda
    Knob* top[] = { &knobs[0], &knobs[1], &knobs[2] };
    placeRow (bounds.removeFromTop (170).reduced (40, 0), top, 3);

    separatorY = bounds.getY() + 10;
    bounds.removeFromTop (20);

    // Fila 2: frecuencias de corte, centradas
    Knob* bottom[] = { &knobs[3], &knobs[4] };
    placeRow (bounds.removeFromTop (140).reduced (140, 0), bottom, 2);
}
