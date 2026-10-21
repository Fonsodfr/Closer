/*
  ==============================================================================

    Closer - Rave Factory
    Paleta y tipografías Rave Factory (compartidas por toda la interfaz)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace theme
{
    const juce::Colour lime      (0xffe4f22a);   // amarillo lima Rave Factory
    const juce::Colour green     (0xff8ee36b);   // verde fósforo (Mid)
    const juce::Colour darkGreen (0xff3f7d3a);   // verde oscuro (Side)
    const juce::Colour screen    (0xff0a140a);   // fondo de la pantalla
    const juce::Colour panel     (0xff1a1b1e);
    const juce::Colour header    (0xff111214);
    const juce::Colour line      (0xff2c2e33);

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
    inline juce::Font rfFont (float size, bool bold = true, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions ("Bahnschrift", size, bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kerning));
    }

    // Tipografía de "pantalla" para los valores numéricos
    inline juce::Font lcdFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }
}
