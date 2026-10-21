/*
  ==============================================================================

    Closer - Rave Factory
    Estilo de los mandos y botones

  ==============================================================================
*/

#include "RaveFactoryLookAndFeel.h"
#include "Theme.h"

using namespace theme;

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
