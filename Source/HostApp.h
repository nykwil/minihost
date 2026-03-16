#pragma once

#include <cmath>
#include <vector>
#include <JuceHeader.h>

class HostApp : public juce::AudioIODeviceCallback,
                public juce::MidiInputCallback
{
public:
    HostApp();
    ~HostApp() override;

    bool initialise(const juce::String& pluginPath,
                    const juce::String& configPath = {},
                    double bpmOverride = 0.0);
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
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;

    // Testing mode functionality
    bool runTest();
    void setLooping(bool loopFlag) { shouldLoop = loopFlag; }
    
    juce::AudioPluginInstance* getPluginInstance() const { return pluginInstance.get(); }

private:
    struct AudioInputSlot
    {
        enum class SourceType { File, DeviceChannel };

        int slotIndex = 1;               // slot in config, e.g. audio_1
        int pluginInputChannel = 0;      // zero-based channel on plugin input
        SourceType sourceType = SourceType::File;

        juce::String filePath;
        int requestedDeviceId = 0;       // one-based in config, 0 is also accepted as first channel
        int deviceInputChannel = 0;      // zero-based input channel index
        bool warnedInvalidChannel = false;

        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        std::unique_ptr<juce::AudioTransportSource> transportSource;
    };

    struct MidiInputSlot
    {
        enum class SourceType { File, Device };

        int slotIndex = 1;               // slot in config, e.g. midi_1
        SourceType sourceType = SourceType::File;

        juce::String filePath;
        int requestedDeviceId = 0;       // one-based in config, 0 is also accepted as first device
        juce::String deviceIdentifier;
    };

    class HostTransportPlayHead : public juce::AudioPlayHead
    {
    public:
        void update(double bpmIn,
                    double sampleRateIn,
                    int64_t samplePositionIn,
                    bool isPlayingIn,
                    bool isLoopingIn,
                    int timeSigNumeratorIn = 4,
                    int timeSigDenominatorIn = 4)
        {
            bpm = bpmIn > 0.0 ? bpmIn : 120.0;
            sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
            samplePosition = juce::jmax<int64_t>(0, samplePositionIn);
            isPlaying = isPlayingIn;
            isLooping = isLoopingIn;
            timeSigNumerator = juce::jmax(1, timeSigNumeratorIn);
            timeSigDenominator = juce::jmax(1, timeSigDenominatorIn);
        }

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            const auto seconds = static_cast<double>(samplePosition) / sampleRate;
            const auto ppq = seconds * (bpm / 60.0);
            const auto ppqPerBar = static_cast<double>(timeSigNumerator) * (4.0 / static_cast<double>(timeSigDenominator));

            info.setBpm(bpm);
            info.setTimeInSamples(samplePosition);
            info.setTimeInSeconds(seconds);
            info.setPpqPosition(ppq);
            juce::AudioPlayHead::TimeSignature timeSignature;
            timeSignature.numerator = timeSigNumerator;
            timeSignature.denominator = timeSigDenominator;
            info.setTimeSignature(timeSignature);
            info.setIsPlaying(isPlaying);
            info.setIsRecording(false);
            info.setIsLooping(isLooping);

            if (ppqPerBar > 0.0)
                info.setPpqPositionOfLastBarStart(std::floor(ppq / ppqPerBar) * ppqPerBar);

            return info;
        }

    private:
        double bpm = 120.0;
        double sampleRate = 44100.0;
        int64_t samplePosition = 0;
        bool isPlaying = false;
        bool isLooping = false;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;
    };

    void parseConfig(const juce::String& configPath);
    void applyBpmToMidiSequence(double bpm);
    void logRoutingSummary();
    void preparePlaybackState(double sampleRate, int blockSize);
    void releasePlaybackState();
    void renderNextBlock(const float* const* inputChannelData, int numInputChannels, int numSamples);
    
    bool shouldLoop = false;
    double configuredBpm = 120.0;
    bool hasConfiguredAudioSlots = false;
    bool hasConfiguredMidiSlots = false;
    bool hasLoadedMidiFileSequence = false;

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

    // Audio Playback
    juce::AudioFormatManager audioFormatManager;
    std::vector<AudioInputSlot> audioInputSlots;
    juce::AudioBuffer<float> slotScratchBuffer;
    
    // MIDI Playback
    std::vector<MidiInputSlot> midiInputSlots;
    juce::StringArray activeMidiDeviceIdentifiers;
    juce::MidiMessageCollector midiCollector;
    juce::MidiMessageSequence midiSequence;
    int nextMidiEventIndex = 0;
    int64_t playbackSamplePosition = 0;
    
    juce::AudioBuffer<float> internalBuffer;
    juce::MidiBuffer internalMidiBuffer;
    HostTransportPlayHead hostTransportPlayHead;

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
