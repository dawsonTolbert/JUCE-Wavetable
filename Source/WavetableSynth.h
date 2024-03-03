/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             WavetableSynthTutorial
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Wavetable synthesiser.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once

//==============================================================================
class SineOscillator
{
public:
    SineOscillator() {}

    void setFrequency (float frequency, float sampleRate)
    {
        auto cyclesPerSample = frequency / sampleRate;
        angleDelta = cyclesPerSample * juce::MathConstants<float>::twoPi;
    }

    forcedinline void updateAngle() noexcept
    {
        currentAngle += angleDelta;
        if (currentAngle >= juce::MathConstants<float>::twoPi)
            currentAngle -= juce::MathConstants<float>::twoPi;
    }

    forcedinline float getNextSample() noexcept
    {
        auto currentSample = std::sin (currentAngle);
        updateAngle();
        return currentSample;
    }

private:
    float currentAngle = 0.0f, angleDelta = 0.0f;
};

class WavetableOscillator
{
public:
    WavetableOscillator(const juce::AudioSampleBuffer& wavetableToUse) : wavetable(wavetableToUse) 
    {
        jassert(wavetable.getNumChannels() == 1);
    }

    void setFrequency(float frequency, float sampleRate)
    {
        auto tableSizeOverSampleRate = (float)wavetable.getNumSamples() / sampleRate;
        tableDelta = frequency * tableSizeOverSampleRate;
    }

    forcedinline float getNextSample() noexcept
    {
        auto tableSize = (unsigned int)wavetable.getNumSamples();

        auto index0 = (unsigned int)currentIndex;
        auto index1 = index0 == (tableSize - 1) ? (unsigned int)0 : index0 + 1;

        auto frac = currentIndex - (float)index0;

        auto* table = wavetable.getReadPointer(0);
        auto value0 = table[index0];
        auto value1 = table[index1];

        auto currentSample = value0 + frac * (value1 - value0);

        if ((currentIndex += tableDelta) > (float)tableSize) {
            currentIndex -= (float)tableSize;
        }

        return currentSample;
    }
private:
    const juce::AudioSampleBuffer& wavetable;
    float currentIndex = 0.0f, tableDelta = 0.0f;
};

//==============================================================================
class MainContentComponent   : public juce::AudioAppComponent,
                               public juce::Timer
{
public:
    MainContentComponent()
    {
        cpuUsageLabel.setText ("CPU Usage", juce::dontSendNotification);
        cpuUsageText.setJustificationType (juce::Justification::right);
        addAndMakeVisible (cpuUsageLabel);
        addAndMakeVisible (cpuUsageText);

        createWavetable();

        setSize (400, 200);
        setAudioChannels (0, 2); // no inputs, two outputs
        startTimer (50);
    }

    ~MainContentComponent() override
    {
        shutdownAudio();
    }

    void resized() override
    {
        cpuUsageLabel.setBounds (10, 10, getWidth() - 20, 20);
        cpuUsageText .setBounds (10, 10, getWidth() - 20, 20);
    }

    void timerCallback() override
    {
        auto cpu = deviceManager.getCpuUsage() * 100;
        cpuUsageText.setText (juce::String (cpu, 6) + " %", juce::dontSendNotification);
    }

    void createWavetable()
    {
        sineTable.setSize(1, (int)tableSize);
        auto* samples = sineTable.getWritePointer(0);

        auto angleDelta = juce::MathConstants<double>::twoPi / (double)(tableSize - 1);
        auto currentAngle = 0.0;

        for (unsigned int i = 0; i < tableSize; ++i) {
            auto sample = std::sin(currentAngle);
            samples[i] = (float)sample;
            currentAngle += angleDelta;
        }
    }

    void prepareToPlay (int, double sampleRate) override
    {
        auto numberOfOscillators = 200;                                                 // [1]

        for (auto i = 0; i < numberOfOscillators; ++i)
        {
            auto* oscillator = new WavetableOscillator(sineTable);                                    // [2]


            //make list of possible note values in major chord
            auto majorScale = {48.0, 50.0, 52.0, 53.0, 55.0, 57.0, 59.0, 
                60.0, 62.0, 64.0, 65.0, 67.0, 69.0, 71.0, 
                72.0, 74.0, 76.0, 77.0, 79.0, 81.0, 83.0, 
                84.0};
            //take random midiNote value
            auto midiNote = juce::Random::getSystemRandom().nextDouble() * 36.0 + 48.0; // [3]

            //find closest majorScale value and assign value to midiNote
            auto nearest = majorScale.begin();
            double minDiff = std::abs(*nearest - midiNote);
            for (auto i = majorScale.begin() + 1; i != majorScale.end(); ++i) {
                double diff = std::abs(*i - midiNote);
                if (diff < minDiff) {
                    nearest = i;
                    minDiff = diff;
                }
            }

            midiNote = *nearest;

            auto frequency = 440.0 * pow (2.0, (midiNote - 69.0) / 12.0);               // [4]

            oscillator->setFrequency ((float) frequency, (float) sampleRate);           // [5]
            oscillators.add(oscillator);
        }

        level = 0.25f / (float) numberOfOscillators;                                    // [6]
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* leftBuffer  = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample); // [7]
        auto* rightBuffer = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);

        bufferToFill.clearActiveBufferRegion();

        for (auto oscillatorIndex = 0; oscillatorIndex < oscillators.size(); ++oscillatorIndex)
        {
            auto* oscillator = oscillators.getUnchecked (oscillatorIndex);                      // [8]

            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto levelSample = oscillator->getNextSample() * level;                         // [9]

                leftBuffer[sample]  += levelSample;                                             // [10]
                rightBuffer[sample] += levelSample;
            }
        }
    }

private:
    juce::Label cpuUsageLabel;
    juce::Label cpuUsageText;

    const unsigned int tableSize = 1 << 7;
    float level = 0.0f;

    juce::AudioSampleBuffer sineTable;
    juce::OwnedArray<WavetableOscillator> oscillators;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
