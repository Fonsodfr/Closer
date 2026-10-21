/*
  ==============================================================================

    Closer - Rave Factory
    Ventana del plugin

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Theme.h"

using namespace theme;

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
