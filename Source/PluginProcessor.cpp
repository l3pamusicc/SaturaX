#include "PluginProcessor.h"
#include "PluginEditor.h"

SaturaXProcessor::SaturaXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    pDrive   = apvts.getRawParameterValue ("drive");
    pPunish  = apvts.getRawParameterValue ("punish");
    pStyle   = apvts.getRawParameterValue ("style");
    pLowCut  = apvts.getRawParameterValue ("lowcut");
    pHighCut = apvts.getRawParameterValue ("highcut");
    pTone    = apvts.getRawParameterValue ("tone");
    pMix     = apvts.getRawParameterValue ("mix");
    pOutput  = apvts.getRawParameterValue ("output");
    pAuto    = apvts.getRawParameterValue ("autogain");
    pMode    = apvts.getRawParameterValue ("mode");
}

AudioProcessorValueTreeState::ParameterLayout SaturaXProcessor::createParameterLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<P> (ParameterID { "drive", 1 }, "Drive",
                 NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.0f,
                 A().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "punish", 1 }, "Punish", false));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style",
                 StringArray { "Tape", "Tube", "Transformer", "Diode", "Fold" }, 0));

    p.push_back (std::make_unique<P> (ParameterID { "lowcut", 1 }, "Low Cut",
                 NormalisableRange<float> (20.0f, 500.0f, 0.1f, 0.4f), 20.0f,
                 A().withStringFromValueFunction ([] (float v, int)
                 { return v <= 21.0f ? String ("Off") : String (roundToInt (v)) + " Hz"; })
                    .withValueFromStringFunction ([] (const String& s)
                 { return s.containsIgnoreCase ("off") ? 20.0f : s.getFloatValue(); })));

    p.push_back (std::make_unique<P> (ParameterID { "highcut", 1 }, "High Cut",
                 NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 20000.0f,
                 A().withStringFromValueFunction ([] (float v, int)
                 { return v >= 19900.0f ? String ("Off")
                                         : (v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"); })
                    .withValueFromStringFunction ([] (const String& s)
                 { if (s.containsIgnoreCase ("off")) return 20000.0f;
                   auto v = s.getFloatValue(); return s.containsIgnoreCase ("k") ? v * 1000.0f : v; })));

    p.push_back (std::make_unique<P> (ParameterID { "tone", 1 }, "Tone",
                 NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                 A().withStringFromValueFunction ([] (float v, int)
                 { return (v > 0.5f ? "+" : "") + String (roundToInt (v)); })));

    p.push_back (std::make_unique<P> (ParameterID { "mix", 1 }, "Mix",
                 NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f,
                 A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })));

    p.push_back (std::make_unique<P> (ParameterID { "output", 1 }, "Output",
                 NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                 A().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto Gain", true));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                 StringArray { "Stereo", "Mid", "Side" }, 0));

    return { p.begin(), p.end() };
}

void SaturaXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = jmax (samplesPerBlock, 64);

    dsp::ProcessSpec spec { sr, (uint32) maxBlock, 2 };

    oversampling.initProcessing ((size_t) maxBlock);
    oversampling.reset();
    const int latency = roundToInt (oversampling.getLatencyInSamples());
    setLatencySamples (latency);

    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();

    for (auto* f : { &lowCut, &dcBlock, &highCut, &toneLP })
    {
        f->prepare (spec);
        f->setResonance (0.7071f);
        f->reset();
    }
    lowCut.setType  (dsp::StateVariableTPTFilterType::highpass);
    dcBlock.setType (dsp::StateVariableTPTFilterType::highpass);
    dcBlock.setCutoffFrequency (10.0f);
    highCut.setType (dsp::StateVariableTPTFilterType::lowpass);
    toneLP.setType  (dsp::StateVariableTPTFilterType::lowpass);
    toneLP.setCutoffFrequency (800.0f);

    dryBuf.setSize (2, maxBlock);

    driveDbS.reset (sr, 0.02); mixS.reset (sr, 0.02); outDbS.reset (sr, 0.02);
    driveDbS.setCurrentAndTargetValue (Sat::driveDb (*pDrive, *pPunish > 0.5f));
    mixS.setCurrentAndTargetValue (*pMix * 0.01f);
    outDbS.setCurrentAndTargetValue (*pOutput);

    lastMode = (int) *pMode;
}

bool SaturaXProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo()
        && l.getMainInputChannelSet()  == AudioChannelSet::stereo();
}

void SaturaXProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;

    const int total = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const int  mode  = (int) *pMode;               // 0 Stereo, 1 Mid, 2 Side
    const int  style = (int) *pStyle;
    const bool punish   = *pPunish > 0.5f;
    const bool autoGain = *pAuto > 0.5f;
    const bool sat0 = (mode == 0 || mode == 1);
    const bool sat1 = (mode == 0 || mode == 2);

    if (mode != lastMode)
    {
        for (auto* f : { &lowCut, &dcBlock, &highCut, &toneLP }) f->reset();
        lastMode = mode;
    }

    const float driveDb = Sat::driveDb (*pDrive, punish);
    driveDbS.setTargetValue (driveDb);
    mixS.setTargetValue (*pMix * 0.01f);
    outDbS.setTargetValue (*pOutput + (autoGain ? -0.4f * driveDb : 0.0f));

    const float lowHz = *pLowCut, highHz = *pHighCut;
    const bool lowOn  = lowHz > 21.0f;
    const bool highOn = highHz < 19900.0f && highHz < (float) sr * 0.45f;
    if (lowOn)  lowCut.setCutoffFrequency (lowHz);
    if (highOn) highCut.setCutoffFrequency (highHz);

    const float tone  = *pTone * 0.01f;
    const float gLow  = Decibels::decibelsToGain (-tone * 7.0f);
    const float gHigh = Decibels::decibelsToGain ( tone * 7.0f);

    float* d0 = dryBuf.getWritePointer (0);
    float* d1 = dryBuf.getWritePointer (1);

    for (int start = 0; start < total; start += maxBlock)
    {
        const int len = jmin (maxBlock, total - start);
        float* l = L + start;
        float* r = R + start;

        // 1. Viivästetty kuiva signaali (latenssikompensaatio)
        for (int i = 0; i < len; ++i)
        {
            dryDelay.pushSample (0, l[i]); d0[i] = dryDelay.popSample (0);
            dryDelay.pushSample (1, r[i]); d1[i] = dryDelay.popSample (1);
        }

        // 2. Mid/Side-koodaus
        if (mode > 0)
            for (int i = 0; i < len; ++i)
            {
                const float m = (l[i] + r[i]) * 0.5f, s = (l[i] - r[i]) * 0.5f;
                l[i] = m; r[i] = s;
            }

        // 3. Low cut + drive
        for (int i = 0; i < len; ++i)
        {
            const float g = Decibels::decibelsToGain (driveDbS.getNextValue());
            if (sat0) { float x = l[i]; if (lowOn) x = lowCut.processSample (0, x); l[i] = x * g; }
            if (sat1) { float x = r[i]; if (lowOn) x = lowCut.processSample (1, x); r[i] = x * g; }
        }

        // 4. Saturaatio 4x oversamplauksella
        float* ptrs[2] = { l, r };
        dsp::AudioBlock<float> block (ptrs, 2, (size_t) len);
        auto up = oversampling.processSamplesUp (block);
        for (int c = 0; c < 2; ++c)
        {
            if ((c == 0 && ! sat0) || (c == 1 && ! sat1)) continue;
            float* d = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = Sat::shape (d[i], style);
        }
        oversampling.processSamplesDown (block);

        // 5. DC-esto, tone, high cut
        for (int i = 0; i < len; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                if ((c == 0 && ! sat0) || (c == 1 && ! sat1)) continue;
                float* d = (c == 0) ? l : r;
                float x = dcBlock.processSample (c, d[i]);
                const float lowBand = toneLP.processSample (c, x);
                x = lowBand * gLow + (x - lowBand) * gHigh;
                if (highOn) x = highCut.processSample (c, x);
                d[i] = x;
            }
        }

        // 6. Mid/Side-dekoodaus
        if (mode > 0)
            for (int i = 0; i < len; ++i)
            {
                const float m = l[i], s = r[i];
                l[i] = m + s; r[i] = m - s;
            }

        // 7. Mix + output
        for (int i = 0; i < len; ++i)
        {
            const float mix = mixS.getNextValue();
            const float g   = Decibels::decibelsToGain (outDbS.getNextValue());
            l[i] = (d0[i] * (1.0f - mix) + l[i] * mix) * g;
            r[i] = (d1[i] * (1.0f - mix) + r[i] * mix) * g;
        }
    }
}

AudioProcessorEditor* SaturaXProcessor::createEditor() { return new SaturaXEditor (*this); }

void SaturaXProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SaturaXProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SaturaXProcessor(); }
