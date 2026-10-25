#include "PluginEditor.h"

static const Colour kBg     (0xff15120f);
static const Colour kPanel  (0xff1f1a16);
static const Colour kAccent (0xffff5a36);
static const Colour kText   (0xffe9e2dc);
static const Colour kDim    (0xff8a7f76);

// ---------------------------------------------------------------- LookAndFeel
SatLookAndFeel::SatLookAndFeel()
{
    setColour (TextButton::buttonColourId,   Colour (0xff2a2420));
    setColour (TextButton::buttonOnColourId, kAccent);
    setColour (TextButton::textColourOffId,  kDim);
    setColour (TextButton::textColourOnId,   Colours::white);
    setColour (ComboBox::backgroundColourId, Colour (0xff2a2420));
    setColour (ComboBox::textColourId,       kText);
    setColour (ComboBox::outlineColourId,    Colour (0xff4a3f38));
    setColour (ComboBox::arrowColourId,      kAccent);
    setColour (PopupMenu::backgroundColourId, Colour (0xff2a2420));
    setColour (PopupMenu::textColourId,       kText);
}

void SatLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float radius = d * 0.5f, arcW = jmax (4.0f, d * 0.045f), ar = radius - arcW * 0.5f;
    const float ang = startAngle + pos * (endAngle - startAngle);

    PathStrokeType st (arcW, PathStrokeType::curved, PathStrokeType::rounded);

    Path bg;  bg.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff332b26)); g.strokePath (bg, st);

    Path val; val.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, ang, true);
    g.setColour (kAccent); g.strokePath (val, st);

    const float br = radius - arcW - 5.0f;
    g.setGradientFill (ColourGradient (Colour (0xff3a322c), c.x, c.y - br, Colour (0xff1a1613), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (Colour (0xff4a3f38)); g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    Path p; p.addRoundedRectangle (-2.0f, -br + 4.0f, 4.0f, br * 0.4f, 2.0f);
    p.applyTransform (AffineTransform::rotation (ang).translated (c.x, c.y));
    g.setColour (kText); g.fillPath (p);
}

void SatLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? kAccent : Colour (0xff2a2420));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? kAccent.brighter (0.3f) : Colour (0xff4a3f38));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    g.setColour (on ? Colours::white : kDim);
    g.setFont (Font (FontOptions (13.0f, Font::bold)));
    g.drawText (b.getButtonText(), r.toNearestInt(), Justification::centred);
}

// ---------------------------------------------------------------- Curve
void CurveGraph::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (kPanel); g.fillRoundedRectangle (area, 8.0f);
    auto r = area.reduced (8.0f);

    g.setColour (Colour (0xff332b26));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.drawVerticalLine   ((int) r.getCentreX(), r.getY(), r.getBottom());

    const float drive = *proc.apvts.getRawParameterValue ("drive");
    const bool punish = *proc.apvts.getRawParameterValue ("punish") > 0.5f;
    const int  style  = (int) *proc.apvts.getRawParameterValue ("style");
    const float gain  = Decibels::decibelsToGain (Sat::driveDb (drive, punish));

    Path p;
    constexpr int N = 240;
    for (int i = 0; i <= N; ++i)
    {
        const float xin = -1.0f + 2.0f * (float) i / N;
        const float y = jlimit (-1.2f, 1.2f, Sat::shape (xin * gain, style));
        const float px = r.getX() + r.getWidth() * (float) i / N;
        const float py = r.getCentreY() - y / 1.2f * (r.getHeight() * 0.5f);
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (kAccent);
    g.strokePath (p, PathStrokeType (2.0f));

    g.setColour (kDim);
    g.setFont (11.0f);
    g.drawText ("TRANSFER CURVE", r.toNearestInt().removeFromTop (14), Justification::left);
}

// ---------------------------------------------------------------- Editor
SaturaXEditor::SaturaXEditor (SaturaXProcessor& p)
    : AudioProcessorEditor (&p), proc (p), curve (p)
{
    setSize (640, 430);
    setLookAndFeel (&laf);

    addAndMakeVisible (curve);

    addKnob (drive,   "drive",   "DRIVE");
    addKnob (lowcut,  "lowcut",  "LOW CUT");
    addKnob (highcut, "highcut", "HIGH CUT");
    addKnob (tone,    "tone",    "TONE");
    addKnob (mix,     "mix",     "MIX");
    addKnob (output,  "output",  "OUTPUT");

    const char* names[5] = { "TAPE", "TUBE", "XFMR", "DIODE", "FOLD" };
    for (int i = 0; i < 5; ++i)
    {
        styleBtn[i].setButtonText (names[i]);
        styleBtn[i].setClickingTogglesState (true);
        styleBtn[i].setRadioGroupId (101);
        styleBtn[i].onClick = [this, i] { styleAttach->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (styleBtn[i]);
    }
    styleAttach = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter ("style"),
        [this] (float v)
        {
            const int idx = roundToInt (v);
            for (int i = 0; i < 5; ++i)
                styleBtn[i].setToggleState (i == idx, dontSendNotification);
        });
    styleAttach->sendInitialUpdate();

    addAndMakeVisible (punishBtn);
    addAndMakeVisible (autoBtn);
    punishAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "punish", punishBtn);
    autoAttach   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autogain", autoBtn);

    modeBox.addItemList ({ "Stereo (L/R)", "Mid", "Side" }, 1);
    modeBox.setJustificationType (Justification::centred);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", modeBox);

    modeLabel.setText ("PROCESS", dontSendNotification);
    modeLabel.setColour (Label::textColourId, kDim);
    modeLabel.setFont (Font (FontOptions (11.0f, Font::bold)));
    addAndMakeVisible (modeLabel);
}

SaturaXEditor::~SaturaXEditor() { setLookAndFeel (nullptr); }

void SaturaXEditor::addKnob (Knob& k, const String& id, const String& name)
{
    k.name = name;
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 20);
    k.slider.setColour (Slider::textBoxTextColourId, kText);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);
    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void SaturaXEditor::paint (Graphics& g)
{
    g.fillAll (kBg);

    g.setColour (kText);
    g.setFont (Font (FontOptions (26.0f, Font::bold)));
    g.drawText ("SATURAX", 20, 10, 240, 32, Justification::left);
    g.setColour (kAccent);
    g.fillRect (22, 44, 40, 3);
    g.setColour (kDim);
    g.setFont (11.0f);
    g.drawText ("saturation", 70, 38, 120, 14, Justification::left);

    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.setColour (kDim);
    g.drawText (drive.name, drive.slider.getX(), drive.slider.getY() - 14, drive.slider.getWidth(), 14,
                Justification::centred);
    for (auto* k : { &lowcut, &highcut, &tone, &mix, &output })
        g.drawText (k->name, k->slider.getX(), k->slider.getY() - 14, k->slider.getWidth(), 14,
                    Justification::centred);
}

void SaturaXEditor::resized()
{
    drive.slider.setBounds (20, 78, 230, 214);

    curve.setBounds (270, 60, 350, 90);

    const int bw = 350 / 5;
    for (int i = 0; i < 5; ++i)
        styleBtn[i].setBounds (270 + i * bw + 2, 160, bw - 4, 30);

    modeLabel.setBounds (270, 202, 60, 26);
    modeBox.setBounds   (335, 202, 115, 26);
    autoBtn.setBounds   (465, 202, 155, 26);

    punishBtn.setBounds (270, 244, 350, 38);

    const int w = 600 / 5;
    int x = 20;
    for (auto* k : { &lowcut, &highcut, &tone, &mix, &output })
    {
        k->slider.setBounds (x, 312, w, 108);
        x += w;
    }
}
