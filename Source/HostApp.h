#pragma once

#include <JuceHeader.h>

class HostApp : public juce::AudioIODeviceCallback
{
public:
    HostApp();
    ~HostApp() override;

    bool initialise(const juce::String& pluginPath);
    void shutdown();
    
    // AudioIODeviceCallback methods
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Testing mode functionality
    bool runTest();
    void setLooping(bool loopFlag) { shouldLoop = loopFlag; }
    
    juce::AudioPluginInstance* getPluginInstance() const { return pluginInstance.get(); }

private:
    void parseConfig();
    void generateInternalTestSequence(double sampleRate);
    
    bool shouldLoop = false;
    juce::String audioFilePath;
    juce::String midiFilePath;

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

    // Audio Playback
    juce::AudioFormatManager audioFormatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> audioReaderSource;
    std::unique_ptr<juce::AudioTransportSource> audioTransportSource;
    
    // MIDI Playback
    juce::MidiMessageSequence midiSequence;
    int nextMidiEventIndex = 0;
    int64_t playbackSamplePosition = 0;
    
    juce::AudioBuffer<float> internalBuffer;
    juce::MidiBuffer internalMidiBuffer;

    // Fallback sine wave (used when no audio file is configured)
    double sinePhase = 0.0;
    double sineFrequency = 440.0; // A4

    // Fallback MIDI pattern state (used when no MIDI file is configured)
    // Pattern: noteOn -> hold -> noteOff -> rest -> repeat
    enum class FallbackMidiState { NoteOn, Holding, NoteOff, Resting };
    FallbackMidiState fallbackMidiState = FallbackMidiState::NoteOn;
    int64_t fallbackMidiSampleCounter = 0;
    int64_t fallbackNoteLengthSamples = 0;   // set in audioDeviceAboutToStart
    int64_t fallbackRestLengthSamples = 0;
    int fallbackMidiNote = 60; // middle C

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostApp)
};
