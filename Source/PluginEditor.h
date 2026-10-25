#pragma once
#include "PluginProcessor.h"

class CurveGraph : public Component, private Timer
{
public:
    explicit CurveGraph (SaturaXProcessor& p) : proc (p) { startTimerHz (20); }
    void paint (Graphics& g) override;
private:
    void timerCallback() override { repaint(); }
    SaturaXProcessor& proc;
};

class SatLookAndFeel : public LookAndFeel_V4
{
public:
    SatLookAndFeel();
    void drawRotarySlider (Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, Slider&) override;
    void drawToggleButton (Graphics&, ToggleButton&, bool highlighted, bool down) override;
};

class SaturaXEditor : public AudioProcessorEditor
{
public:
    explicit SaturaXEditor (SaturaXProcessor&);
    ~SaturaXEditor() override;

    void paint (Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        Slider slider;
        std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attach;
        String name;
    };

    void addKnob (Knob& k, const String& paramId, const String& name);

    SaturaXProcessor& proc;
    SatLookAndFeel laf;
    CurveGraph curve;

    Knob drive, lowcut, highcut, tone, mix, output;

    TextButton styleBtn[5];
    std::unique_ptr<ParameterAttachment> styleAttach;

    ToggleButton punishBtn { "PUNISH" }, autoBtn { "AUTO GAIN" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> punishAttach, autoAttach;

    ComboBox modeBox;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    Label modeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturaXEditor)
};
