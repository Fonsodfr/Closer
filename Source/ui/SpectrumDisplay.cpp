/*
  ==============================================================================

    Closer - Rave Factory
    Pantalla de espectro (Mid y Side)

  ==============================================================================
*/

#include "SpectrumDisplay.h"
#include "Theme.h"

using namespace theme;

namespace
{
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

    // Opaco = pinta todo su rectángulo. Así, al redibujarse 30 veces por segundo,
    // JUCE no tiene que redibujar también el panel que hay detrás.
    setOpaque (true);

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

    // Fondo de pantalla verde oscuro, con el centro un poco más claro (efecto pantalla CRT)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff12240f), w * 0.5f, h * 0.4f,
                                             screen, 0.0f, h, true));
    g.fillAll();

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

    // Sombra interior en los bordes: la pantalla parece hundida en el panel
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (area, 2.0f);
}
