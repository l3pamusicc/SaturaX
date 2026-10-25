#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Saturation.h"

using namespace juce;

class SaturaXProcessor : public AudioProcessor
{
public:
    SaturaXProcessor();
    ~SaturaXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return "SaturaX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AudioProcessorValueTreeState apvts;

private:
    static AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float> *pDrive {}, *pPunish {}, *pStyle {}, *pLowCut {}, *pHighCut {},
                       *pTone {}, *pMix {}, *pOutput {}, *pAuto {}, *pMode {};

    dsp::Oversampling<float> oversampling { 2, 2, dsp::Oversampling<float>::filterHalfBandFIREquiriple, true, true };

    dsp::StateVariableTPTFilter<float> lowCut, dcBlock, highCut, toneLP;
    dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    AudioBuffer<float> dryBuf;

    SmoothedValue<float> driveDbS, mixS, outDbS;

    double sr = 44100.0;
    int maxBlock = 512;
    int lastMode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturaXProcessor)
};
