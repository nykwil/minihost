#include <JuceHeader.h>
#include "HostApp.h"
HostApp::HostApp()
{
    audioFormatManager.registerBasicFormats();
    formatManager.addFormat(new juce::VST3PluginFormat());
}

HostApp::~HostApp()
{
    shutdown();
}

bool HostApp::initialise(const juce::String& pluginPath)
{
    juce::Logger::writeToLog("HostApp::initialise called.");
    
    // Parse minihost_config.json for audio/midi file paths
    parseConfig();
    // Initialize standard audio and MIDI devices
    auto setup = deviceManager.getAudioDeviceSetup();
    juce::Logger::writeToLog("Initializing default audio devices...");
    auto err = deviceManager.initialiseWithDefaultDevices(2, 2);
    juce::Logger::writeToLog("Audio devices initialized.");
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("Error initializing audio devices: " + err);
        return false;
    }

    // Enable MIDI input from all available devices
    juce::Logger::writeToLog("Getting MIDI inputs...");
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& input : midiInputs)
    {
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
    }

    // We no longer use processorPlayer.
    // Instead we drive the AudioIODeviceCallback directly.
    // deviceManager.addMidiInputDeviceCallback({}, &processorPlayer);

    juce::File pluginFile(pluginPath);
    if (!pluginFile.existsAsFile() && !pluginFile.isDirectory())
    {
        juce::Logger::writeToLog("Plugin file does not exist: " + pluginPath);
        return false;
    }

    juce::KnownPluginList pluginList;
    juce::VST3PluginFormat vst3Format;

    juce::String fileOrId = pluginFile.getFullPathName();
    juce::Logger::writeToLog("Scanning: " + fileOrId);

    juce::PluginDirectoryScanner scanner(pluginList, vst3Format, {}, true, juce::File());

    juce::OwnedArray<juce::PluginDescription> typesFound;
    
    // Instead of instantiating blindly, let's scan it as a VST3 explicitly
    if (!vst3Format.fileMightContainThisPluginType(fileOrId))
    {
        juce::Logger::writeToLog("Warning: fileMightContainThisPluginType returned false.");
    }
    
    vst3Format.findAllTypesForFile(typesFound, fileOrId);

    if (typesFound.size() == 0)
    {
        juce::Logger::writeToLog("Failed to find any internal plugin types in file: " + fileOrId);
        return false;
    }

    juce::Logger::writeToLog("Found " + juce::String(typesFound.size()) + " plugin types inside file. Loading the first one...");

    juce::String errorMessage;
    pluginInstance = formatManager.createPluginInstance(*(typesFound[0]), 44100.0, 512, errorMessage);

    if (pluginInstance == nullptr)
    {
        juce::Logger::writeToLog("Failed to load plugin (" + pluginPath + "): " + errorMessage);
        return false;
    }

    juce::Logger::writeToLog("Successfully loaded plugin: " + pluginInstance->getName());

    // Connect audio callback directly to HostApp
    deviceManager.addAudioCallback(this);

    return true;
}

void HostApp::shutdown()
{
    deviceManager.removeAudioCallback(this);
    pluginInstance.reset();
}

bool HostApp::runTest()
{
    if (pluginInstance == nullptr)
        return false;

    juce::Logger::writeToLog("--- Starting Test Mode ---");

    // No live audio callback to disconnect (we are the callback)

    const int numBlocksToProcess = 10;
    const int bufferSize = 512;
    const double sampleRate = 44100.0;

    pluginInstance->prepareToPlay(sampleRate, bufferSize);

    juce::AudioBuffer<float> buffer(pluginInstance->getTotalNumOutputChannels(), bufferSize);
    juce::MidiBuffer midiBuffer;

    for (int i = 0; i < numBlocksToProcess; ++i)
    {
        buffer.clear();
        pluginInstance->processBlock(buffer, midiBuffer);
    }

    juce::Logger::writeToLog("Processed " + juce::String(numBlocksToProcess) + " blocks successfully.");

    // We do not re-add the callback here because the app will shut down immediately after testing
    return true;
}

void HostApp::parseConfig()
{
    juce::File configFile = juce::File::getCurrentWorkingDirectory().getChildFile("minihost_config.json");
    if (!configFile.existsAsFile())
    {
        juce::Logger::writeToLog("No minihost_config.json found, using internal generated sequences.");
        return;
    }

    juce::var config = juce::JSON::parse(configFile.loadFileAsString());
    if (!config.isObject())
    {
        juce::Logger::writeToLog("minihost_config.json is not valid JSON.");
        return;
    }

    if (auto* obj = config.getDynamicObject())
    {
        if (obj->hasProperty("audio_file"))
        {
            audioFilePath = obj->getProperty("audio_file").toString();
            juce::Logger::writeToLog("Config loaded audio_file: " + audioFilePath);
        }
        if (obj->hasProperty("midi_file"))
        {
            midiFilePath = obj->getProperty("midi_file").toString();
            juce::Logger::writeToLog("Config loaded midi_file: " + midiFilePath);
        }
    }
}

void HostApp::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (pluginInstance == nullptr)
        return;

    auto sampleRate = device->getCurrentSampleRate();
    auto blockSize  = device->getCurrentBufferSizeSamples();

    pluginInstance->prepareToPlay (sampleRate, blockSize);
    internalBuffer.setSize(pluginInstance->getTotalNumInputChannels(), blockSize);

    // Initialize audio reading if specified
    if (audioFilePath.isNotEmpty())
    {
        juce::File audioFile(audioFilePath);
        if (auto* reader = audioFormatManager.createReaderFor(audioFile))
        {
            audioReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            audioTransportSource = std::make_unique<juce::AudioTransportSource>();
            audioTransportSource->setSource(audioReaderSource.get(), 0, nullptr, reader->sampleRate);
            audioTransportSource->prepareToPlay(blockSize, sampleRate);
            audioTransportSource->start();
            juce::Logger::writeToLog("Loaded audio file for playback: " + audioFilePath);
        }
        else
        {
            juce::Logger::writeToLog("Failed to read audio file: " + audioFilePath);
        }
    }

    if (midiFilePath.isNotEmpty())
    {
        juce::File midiFile(midiFilePath);
        juce::FileInputStream midiFileStream(midiFile);
        if (midiFileStream.openedOk())
        {
            juce::MidiFile parsedMidi;
            if (parsedMidi.readFrom(midiFileStream))
            {
                parsedMidi.convertTimestampTicksToSeconds();
                int numTracks = parsedMidi.getNumTracks();
                for (int i = 0; i < numTracks; ++i)
                {
                    midiSequence.addSequence(*parsedMidi.getTrack(i), 0.0);
                }
                midiSequence.updateMatchedPairs();
                juce::Logger::writeToLog("Loaded MIDI file for playback: " + midiFilePath);
            }
        }
        else
        {
            juce::Logger::writeToLog("Failed to read MIDI file: " + midiFilePath);
        }
    }
    nextMidiEventIndex = 0;

    // Initialize fallback MIDI timing (120 BPM quarter notes)
    const double bpm = 120.0;
    const double secondsPerBeat = 60.0 / bpm;
    fallbackNoteLengthSamples = static_cast<int64_t>(secondsPerBeat * sampleRate);
    fallbackRestLengthSamples = static_cast<int64_t>(secondsPerBeat * sampleRate);
    fallbackMidiSampleCounter = 0;
    fallbackMidiState = FallbackMidiState::NoteOn;
    sinePhase = 0.0;
}

void HostApp::audioDeviceStopped()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();
        
    if (audioTransportSource != nullptr)
    {
        audioTransportSource->stop();
        audioTransportSource->releaseResources();
    }
}

void HostApp::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                int numInputChannels,
                                                float* const* outputChannelData,
                                                int numOutputChannels,
                                                int numSamples,
                                                const juce::AudioIODeviceCallbackContext& context)
{
    if (pluginInstance == nullptr)
        return;

    // Process audio inputs
    internalBuffer.setSize(pluginInstance->getTotalNumInputChannels(), numSamples, false, false, true);
    internalBuffer.clear();
    internalMidiBuffer.clear();

    if (audioTransportSource != nullptr && audioTransportSource->isPlaying())
    {
        juce::AudioSourceChannelInfo info(&internalBuffer, 0, numSamples);
        audioTransportSource->getNextAudioBlock(info);
        
        if (audioTransportSource->hasStreamFinished())
        {
            if (shouldLoop)
            {
                audioTransportSource->setPosition(0.0);
                audioTransportSource->start();
            }
            else
            {
                audioTransportSource->stop();
            }
        }
    }
    else if (audioFilePath.isEmpty())
    {
        // Fallback: generate a 440Hz sine wave
        const double sampleRate = pluginInstance->getSampleRate();
        const double phaseIncrement = juce::MathConstants<double>::twoPi * sineFrequency / sampleRate;
        const int numChannels = internalBuffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            float sample = 0.5f * static_cast<float>(std::sin(sinePhase));
            for (int ch = 0; ch < numChannels; ++ch)
                internalBuffer.setSample(ch, s, sample);
            sinePhase += phaseIncrement;
            if (sinePhase >= juce::MathConstants<double>::twoPi)
                sinePhase -= juce::MathConstants<double>::twoPi;
        }
    }

    // Prepare MIDI events — file or fallback pattern
    if (midiFilePath.isNotEmpty())
    {
        double timeNow  = playbackSamplePosition / pluginInstance->getSampleRate();
        double nextTime  = timeNow + (numSamples / pluginInstance->getSampleRate());

        while (nextMidiEventIndex < midiSequence.getNumEvents())
        {
            auto* event = midiSequence.getEventPointer(nextMidiEventIndex);
            double eventTime = event->message.getTimeStamp();

            if (eventTime >= timeNow && eventTime < nextTime)
            {
                int samplePos = juce::jlimit(0, numSamples - 1, static_cast<int>((eventTime - timeNow) * pluginInstance->getSampleRate()));
                internalMidiBuffer.addEvent(event->message, samplePos);
                nextMidiEventIndex++;
            }
            else if (eventTime >= nextTime)
            {
                break;
            }
            else 
            {
                // we've passed it somehow or it's out of order
                nextMidiEventIndex++;
            }
        }

        if (nextMidiEventIndex >= midiSequence.getNumEvents())
        {
            if (shouldLoop)
            {
                nextMidiEventIndex = 0;
                playbackSamplePosition = 0;
            }
        }
    }
    else
    {
        // Fallback MIDI: note-on -> hold -> note-off -> rest -> repeat
        for (int s = 0; s < numSamples; ++s)
        {
            switch (fallbackMidiState)
            {
                case FallbackMidiState::NoteOn:
                    internalMidiBuffer.addEvent(
                        juce::MidiMessage::noteOn(1, fallbackMidiNote, (juce::uint8)100), s);
                    fallbackMidiState = FallbackMidiState::Holding;
                    fallbackMidiSampleCounter = 0;
                    break;

                case FallbackMidiState::Holding:
                    ++fallbackMidiSampleCounter;
                    if (fallbackMidiSampleCounter >= fallbackNoteLengthSamples)
                        fallbackMidiState = FallbackMidiState::NoteOff;
                    break;

                case FallbackMidiState::NoteOff:
                    internalMidiBuffer.addEvent(
                        juce::MidiMessage::noteOff(1, fallbackMidiNote), s);
                    fallbackMidiState = FallbackMidiState::Resting;
                    fallbackMidiSampleCounter = 0;
                    break;

                case FallbackMidiState::Resting:
                    ++fallbackMidiSampleCounter;
                    if (fallbackMidiSampleCounter >= fallbackRestLengthSamples)
                        fallbackMidiState = FallbackMidiState::NoteOn;
                    break;
            }
        }
    }

    // Advance playback position
    playbackSamplePosition += numSamples;

    // VST3 Process Block
    pluginInstance->processBlock(internalBuffer, internalMidiBuffer);

    // Copy to output channels
    for (int i = 0; i < numOutputChannels; ++i)
    {
        if (i < internalBuffer.getNumChannels())
        {
            juce::FloatVectorOperations::copy(outputChannelData[i], internalBuffer.getReadPointer(i), numSamples);
        }
        else
        {
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
        }
    }
}
