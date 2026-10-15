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


//==============================================================================
// Paleta y tipografías Rave Factory (las mismas del diseño de 90s Breaks)
//==============================================================================
namespace
{
    const juce::Colour chassisA   (0xff1d1e21);   // rayas del metal cepillado
    const juce::Colour chassisB   (0xff1a1b1e);
    const juce::Colour panelFill  (0xff18191c);   // fondo de cada sección
    const juce::Colour deepLine   (0xff0e0f11);   // bordes oscuros (hundidos)
    const juce::Colour lightLine  (0xff2c2e33);   // brillos (relieve)
    const juce::Colour textMain   (0xffd9dadc);
    const juce::Colour textDim    (0xff7d8087);
    const juce::Colour textFaint  (0xff55585e);
    const juce::Colour lcdFill    (0xff0c150b);   // fondo de las cajitas de valor
    const juce::Colour lcdBorder  (0xff22301e);
    const juce::Colour warning    (0xffff8a2a);   // naranja: fase problemática

    // Bahnschrift es una tipografía condensada que trae Windows 10/11,
    // parecida a la del diseño. Si no existe, JUCE usa una por defecto.
    juce::Font rfFont (float size, bool bold = true, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions ("Bahnschrift", size, bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kerning));
    }

    // Tipografía de "pantalla" para los valores numéricos
    juce::Font lcdFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }
}

//==============================================================================
// RaveFactoryLookAndFeel
//==============================================================================
RaveFactoryLookAndFeel::RaveFactoryLookAndFeel()
{
    // Colores por defecto de las cajitas de valor (estilo pantalla verde)
    setColour (juce::Slider::textBoxTextColourId,       green);
    setColour (juce::Slider::textBoxBackgroundColourId, lcdFill);
    setColour (juce::Slider::textBoxOutlineColourId,    lcdBorder);
    setColour (juce::Slider::textBoxHighlightColourId,  lime.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId,          green);
    setColour (juce::TextButton::textColourOffId,       juce::Colour (0xffa6a8ad));
    setColour (juce::TextButton::textColourOnId,        lime);
}

// Dibuja un mando como los del diseño: cuerpo negro con relieve, anillo de puntos,
// marcas alrededor y una aguja de color que brilla.
void RaveFactoryLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, float startAngle, float endAngle,
                                               juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto  accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // 1. Marcas alrededor: 11 rayitas, más largas en el principio, el centro y el final
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool  major = (i == 0 || i == 5 || i == 10);
        const auto  p1 = centre.getPointOnCircumference (radius + 2.0f, a);
        const auto  p2 = centre.getPointOnCircumference (radius + (major ? 7.0f : 4.5f), a);
        g.setColour (major ? juce::Colour (0xff6a6d73) : juce::Colour (0xff45474c));
        g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
    }

    // 2. Sombra bajo el mando (parece que sobresale del panel)
    const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, 4.0f).expanded (1.0f));

    // 3. Cuerpo con degradado: luz arriba a la izquierda, oscuro abajo
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3d42), centre.x - radius * 0.3f, centre.y - radius * 0.45f,
                                             juce::Colour (0xff141518), centre.x + radius * 0.4f, centre.y + radius * 0.9f, true));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff050506));
    g.drawEllipse (body, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));                  // brillo del borde superior
    juce::Path rim;
    rim.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f,
                       -juce::MathConstants<float>::pi * 0.6f, juce::MathConstants<float>::pi * 0.6f, true);
    g.strokePath (rim, juce::PathStrokeType (1.2f));

    // 4. Anillo interior de puntos
    juce::Path ring, dashedRing;
    ring.addEllipse (body.reduced (radius * 0.2f));
    const float dashes[] = { 2.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashedRing, ring, dashes, 2);
    g.setColour (juce::Colour (0xff3a3c41));
    g.fillPath (dashedRing);

    // 5. Aguja: primero un halo ancho y transparente (el "brillo"), luego la línea
    const auto from = centre.getPointOnCircumference (radius * 0.18f, angle);
    const auto to   = centre.getPointOnCircumference (radius * 0.86f, angle);
    g.setColour (accent.withAlpha (0.25f));
    g.drawLine ({ from, to }, 7.0f);
    g.setColour (accent);
    g.drawLine ({ from, to }, 3.0f);
}

// Cajita de valor bajo cada mando, con tipografía de pantalla
juce::Label* RaveFactoryLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (lcdFont (13.0f));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

// Botón metálico; cuando está activado se enciende en lima
void RaveFactoryLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                   bool isMouseOver, bool isButtonDown)
{
    auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setGradientFill (juce::ColourGradient (juce::Colour (isButtonDown ? 0xff1c1d20 : 0xff2a2c30), 0.0f, r.getY(),
                                             juce::Colour (isButtonDown ? 0xff2a2c30 : 0xff1c1d20), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);

    if (on)
    {
        g.setColour (lime.withAlpha (0.14f));
        g.fillRoundedRectangle (r, 4.0f);
    }
    else if (isMouseOver)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 4.0f);
    }

    g.setColour (on ? lime : deepLine);
    g.drawRoundedRectangle (r, 4.0f, on ? 1.5f : 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));                  // brillo superior
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
}

juce::Font RaveFactoryLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return rfFont (16.0f, true, 0.15f);
}

//==============================================================================
// CorrelationMeter
//==============================================================================
CorrelationMeter::CorrelationMeter (ImagerAudioProcessor& p) : audioProcessor (p)
{
    startTimerHz (30);
}

CorrelationMeter::~CorrelationMeter()
{
    stopTimer();
}

void CorrelationMeter::timerCallback()
{
    const float newValue = juce::jlimit (-1.0f, 1.0f, audioProcessor.getCorrelation());
    if (std::abs (newValue - value) > 0.005f)
    {
        value = newValue;
        repaint();
    }
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Barra de 20 segmentos de -1 a +1, encendida desde el centro hacia el valor
    auto bar = area.removeFromTop (22.0f);
    g.setColour (lcdFill);
    g.fillRect (bar);
    g.setColour (lcdBorder);
    g.drawRect (bar, 1.0f);

    constexpr int numSegments = 20;
    const auto inner = bar.reduced (3.0f);
    const float segW = inner.getWidth() / (float) numSegments;
    const int   lit  = juce::roundToInt (value * 10.0f);                 // de -10 a +10 segmentos

    for (int i = 0; i < numSegments; ++i)
    {
        const int fromCentre = i - numSegments / 2;                       // -10 … +9
        const bool on = (lit >= 0) ? (fromCentre >= 0 && fromCentre < lit)
                                   : (fromCentre < 0 && fromCentre >= lit);
        juce::Colour c (0xff16221a);
        if (on)
            c = (fromCentre >= 0) ? green : warning;

        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (inner.getX() + (float) i * segW + 1.0f, inner.getY(), segW - 2.0f, inner.getHeight()));
    }

    // Escala
    area.removeFromTop (4.0f);
    auto scale = area.removeFromTop (14.0f);
    g.setFont (lcdFont (11.0f));
    g.setColour (textDim);
    g.drawText ("-1", scale, juce::Justification::centredLeft);
    g.drawText ("0",  scale, juce::Justification::centred);
    g.drawText ("+1", scale, juce::Justification::centredRight);

    // Valor numérico
    area.removeFromTop (4.0f);
    g.setFont (lcdFont (18.0f));
    g.setColour (value >= 0.0f ? green : warning);
    g.drawText ((value >= 0.0f ? "+" : "") + juce::String (value, 2), area.removeFromTop (22.0f),
                juce::Justification::centred);
}

//==============================================================================
// Editor
//==============================================================================
namespace
{
    // Medidas de la ventana y de cada zona (en píxeles)
    constexpr int editorW = 900, editorH = 640;
    constexpr int contentX = 48, contentW = 804;

    const juce::Rectangle<int> spectrumPanel   { contentX, 126, contentW, 250 };
    const juce::Rectangle<int> widthPanel      { contentX, 390, 410, 188 };
    const juce::Rectangle<int> crossoverPanel  { contentX + 424, 390, 190, 188 };
    const juce::Rectangle<int> phasePanel      { contentX + 628, 390, 176, 188 };
    constexpr int panelHeaderH = 30;

    // Sección con borde hundido, brillo superior y una franja de título
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& right)
    {
        g.setColour (panelFill);
        g.fillRect (r);
        g.setColour (deepLine);
        g.drawRect (r, 1);
        g.setColour (lightLine.withAlpha (0.7f));
        g.drawHorizontalLine (r.getY() + 1, (float) r.getX() + 1.0f, (float) r.getRight() - 1.0f);

        auto head = r.removeFromTop (panelHeaderH).reduced (14, 0);
        g.setColour (deepLine);
        g.drawHorizontalLine (r.getY(), (float) r.getX(), (float) r.getRight());

        g.setFont (rfFont (14.0f, true, 0.18f));
        g.setColour (juce::Colour (0xffa6a8ad));
        g.drawText (title, head, juce::Justification::centredLeft);
        g.setColour (textDim);
        g.drawText (right, head, juce::Justification::centredRight);
    }

    // Tornillo de panel: círculo con relieve y una ranura inclinada
    void drawScrew (juce::Graphics& g, float cx, float cy)
    {
        const auto r = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ cx, cy });
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (r.translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), cx - 3.0f, cy - 3.0f,
                                                 juce::Colour (0xff121316), cx + 5.0f, cy + 5.0f, true));
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xff0b0c0e));
        g.drawLine (cx - 4.0f, cy + 2.0f, cx + 4.0f, cy - 2.0f, 1.6f);
    }
}

ImagerAudioProcessorEditor::ImagerAudioProcessorEditor (ImagerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), spectrum (p), phaseMeter (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (phaseMeter);

    // "MIDS" (frecuencias medias) para no confundirlo con la señal Mid (el centro)
    setupKnob (knobs[0], "bassWidth", "LOW",  lime);
    setupKnob (knobs[1], "midWidth",  "MIDS", lime);
    setupKnob (knobs[2], "highWidth", "HIGH", lime);
    setupKnob (knobs[3], "lowXover",  "LOW",  green);
    setupKnob (knobs[4], "highXover", "HIGH", green);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), "bypass", bypassButton);

    setSize (editorW, editorH);
}

ImagerAudioProcessorEditor::~ImagerAudioProcessorEditor()
{
    setLookAndFeel (nullptr);   // obligatorio en JUCE antes de destruir un LookAndFeel propio
}

void ImagerAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& paramID,
                                            const juce::String& title, juce::Colour colour)
{
    // Primero se añade a la ventana: así hereda el LookAndFeel Rave Factory ANTES de
    // crear su cajita de valor (si no, la cajita saldría con el estilo por defecto).
    addAndMakeVisible (knob.slider);
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 20);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId, colour);   // color de la aguja

    // Enlace bidireccional entre el mando y el parámetro del DSP
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), paramID, knob.slider);

    knob.label.setText (title, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (rfFont (14.0f, true, 0.12f));
    knob.label.setColour (juce::Label::textColourId, textMain);
    addAndMakeVisible (knob.label);
}

//==============================================================================
void ImagerAudioProcessorEditor::paint (juce::Graphics& g)
{
    drawBackground (g);
}

// Todo lo que no se mueve. Se dibuja directamente (no como imagen guardada) para que
// se vea nítido también con el escalado de pantalla de Windows al 125 % o 150 %.
void ImagerAudioProcessorEditor::drawBackground (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0b0d));

    // --- Chasis de metal cepillado ---------------------------------------
    const auto chassis = getLocalBounds().reduced (14).toFloat();
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path shape;
        shape.addRoundedRectangle (chassis, 14.0f);
        g.reduceClipRegion (shape);

        for (int yy = (int) chassis.getY(); yy < (int) chassis.getBottom(); yy += 2)   // rayas del cepillado
        {
            g.setColour (((yy / 2) % 2 == 0) ? chassisA : chassisB);
            g.fillRect ((int) chassis.getX(), yy, (int) chassis.getWidth(), 2);
        }
    }
    g.setColour (juce::Colour (0xff34363b));
    g.drawRoundedRectangle (chassis, 14.0f, 2.0f);
    g.setColour (juce::Colour (0xff3a3c41));
    g.drawHorizontalLine ((int) chassis.getY() + 2, chassis.getX() + 14.0f, chassis.getRight() - 14.0f);

    drawScrew (g, chassis.getX() + 13.0f,     chassis.getY() + 13.0f);
    drawScrew (g, chassis.getRight() - 13.0f, chassis.getY() + 13.0f);
    drawScrew (g, chassis.getX() + 13.0f,     chassis.getBottom() - 13.0f);
    drawScrew (g, chassis.getRight() - 13.0f, chassis.getBottom() - 13.0f);

    // --- Cabecera: logo RF, nombre y marca ------------------------------
    {
        // Cuadrado lima inclinado con "RF", como en 90s Breaks
        const auto logo = juce::Rectangle<float> ((float) contentX, 40.0f, 58.0f, 58.0f);
        juce::Graphics::ScopedSaveState state (g);
        const float shear = -0.12f;
        g.addTransform (juce::AffineTransform::shear (shear, 0.0f).translated (-shear * logo.getCentreY(), 0.0f));
        g.setColour (lime);
        g.fillRect (logo);
        g.setColour (juce::Colour (0xff111111));
        g.setFont (rfFont (30.0f, true));
        g.drawText ("RF", logo, juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xffecece6));
    g.setFont (rfFont (46.0f, true, 0.06f));
    g.drawText ("CLOSER", juce::Rectangle<int> (contentX + 76, 36, 300, 48), juce::Justification::centredLeft);

    juce::AttributedString subtitle;
    subtitle.append ("STEREO IMAGER", rfFont (14.0f, true, 0.25f), lime);
    subtitle.append ("  /  3-BAND MID-SIDE", rfFont (14.0f, true, 0.25f), textDim);
    subtitle.draw (g, juce::Rectangle<float> ((float) contentX + 77.0f, 82.0f, 400.0f, 18.0f));

    const int right = contentX + contentW;
    g.setColour (juce::Colour (0xffc9cacd));
    g.setFont (rfFont (24.0f, true, 0.04f));
    g.drawText ("RAVE FACTORY", juce::Rectangle<int> (right - 220, 44, 220, 28), juce::Justification::centredRight);
    g.setColour (textFaint);
    g.setFont (rfFont (12.0f, false, 0.35f));
    g.drawText ("AUDIO TOOLS", juce::Rectangle<int> (right - 220, 72, 220, 16), juce::Justification::centredRight);

    // LED lima encendido, con halo
    const juce::Point<float> led ((float) right - 196.0f, 64.0f);
    g.setColour (lime.withAlpha (0.18f));
    g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (led));
    g.setColour (lime);
    g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (led));

    // Línea grabada bajo la cabecera
    g.setColour (deepLine);
    g.drawHorizontalLine (113, (float) contentX, (float) right);
    g.setColour (lightLine);
    g.drawHorizontalLine (114, (float) contentX, (float) right);

    // --- Paneles ---------------------------------------------------------
    drawPanel (g, spectrumPanel,  "SPECTRUM MONITOR", "+4.5 dB/OCT");
    drawPanel (g, widthPanel,     "WIDTH PER BAND",   "0% = MONO");
    drawPanel (g, crossoverPanel, "CROSSOVER",        "LR4");
    drawPanel (g, phasePanel,     "PHASE",            "");

    // Marco de la pantalla de espectro (bisel verde oscuro alrededor)
    const auto screenBounds = spectrum.getBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2f24));
    g.drawRoundedRectangle (screenBounds.expanded (3.0f), 4.0f, 3.0f);

    // --- Pie --------------------------------------------------------------
    g.setFont (rfFont (11.0f, false, 0.35f));
    g.setColour (textFaint);
    g.drawText ("LR4 CROSSOVER  /  24 dB OCT", juce::Rectangle<int> (contentX, 588, 300, 16), juce::Justification::centredLeft);
    g.drawText ("MADE FOR VINYL", juce::Rectangle<int> (right - 300, 588, 300, 16), juce::Justification::centredRight);
}

void ImagerAudioProcessorEditor::resized()
{
    // Pantalla de espectro, dentro de su panel
    spectrum.setBounds (spectrumPanel.withTrimmedTop (panelHeaderH).reduced (14, 12));

    // Anchura por banda: tres mandos grandes
    {
        auto area = widthPanel.withTrimmedTop (panelHeaderH).reduced (10, 8);
        const int cellW = area.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = area.removeFromLeft (cellW);
            knobs[(size_t) i].label .setBounds (cell.removeFromBottom (20));
            knobs[(size_t) i].slider.setBounds (cell.withSizeKeepingCentre (120, cell.getHeight()));
        }
    }

    // Cortes de frecuencia: dos mandos pequeños
    {
        auto area = crossoverPanel.withTrimmedTop (panelHeaderH).reduced (6, 14);
        const int cellW = area.getWidth() / 2;
        for (int i = 3; i < 5; ++i)
        {
            auto cell = area.removeFromLeft (cellW);
            knobs[(size_t) i].label .setBounds (cell.removeFromBottom (20));
            knobs[(size_t) i].slider.setBounds (cell.reduced (2, 8));
        }
    }

    // Medidor de fase y bypass
    {
        auto area = phasePanel.withTrimmedTop (panelHeaderH).reduced (14, 14);
        phaseMeter.setBounds (area.removeFromTop (66));
        bypassButton.setBounds (area.removeFromBottom (40));
    }
}
