#include <JuceHeader.h>
#include <cmath>
#include <limits>
#include <map>
#include "HostApp.h"

namespace
{
class ScopedPluginPlayHead
{
public:
    ScopedPluginPlayHead(juce::AudioProcessor& processorIn, juce::AudioPlayHead& playHeadIn)
        : processor(processorIn), shouldSetPlayHead(processor.getPlayHead() == nullptr)
    {
        if (shouldSetPlayHead)
            processor.setPlayHead(&playHeadIn);
    }

    ~ScopedPluginPlayHead()
    {
        if (shouldSetPlayHead)
            processor.setPlayHead(nullptr);
    }

private:
    juce::AudioProcessor& processor;
    bool shouldSetPlayHead = false;
};

bool tryParseIndexedSlotKey(const juce::Identifier& key, const juce::String& prefix, int& slotIndexOut)
{
    const auto keyText = key.toString();
    const auto expectedPrefix = prefix + "_";
    if (!keyText.startsWithIgnoreCase(expectedPrefix))
        return false;

    const auto slotText = keyText.substring(expectedPrefix.length());
    if (slotText.isEmpty() || !slotText.containsOnly("0123456789"))
        return false;

    const auto slotIndex = slotText.getIntValue();
    if (slotIndex <= 0)
        return false;

    slotIndexOut = slotIndex;
    return true;
}

bool tryParseIntegerId(const juce::var& value, int& idOut)
{
    if (value.isInt() || value.isInt64())
    {
        idOut = static_cast<int>(value);
        return true;
    }

    if (value.isDouble())
    {
        const auto valueAsDouble = static_cast<double>(value);
        if (std::floor(valueAsDouble) == valueAsDouble)
        {
            idOut = static_cast<int>(valueAsDouble);
            return true;
        }
    }

    return false;
}

int resolveRequestedIdToZeroBasedIndex(int requestedId)
{
    // Config IDs are intended to be one-based (1 = first channel/device), but we accept 0 as first too.
    return requestedId > 0 ? requestedId - 1 : requestedId;
}
}

HostApp::HostApp()
{
    audioFormatManager.registerBasicFormats();
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
}

HostApp::~HostApp()
{
    shutdown();
}

bool HostApp::initialise(const juce::String& pluginPath,
                         const juce::String& configPath,
                         double bpmOverride,
                         bool skipAudioDevice)
{
    juce::Logger::writeToLog("HostApp::initialise called.");

    parseConfig(configPath);

    if (bpmOverride > 0.0)
    {
        configuredBpm = bpmOverride;
        juce::Logger::writeToLog("BPM override applied from command line: " + juce::String(configuredBpm, 2));
    }

    // Only request live audio input channels if any slots are sourced from a device.
    // File-backed slots don't need an input device open.
    bool needsDeviceInput = std::any_of(audioInputSlots.begin(), audioInputSlots.end(),
        [](const AudioInputSlot& s) { return s.sourceType == AudioInputSlot::SourceType::DeviceChannel; });
    int numInputChannels = needsDeviceInput ? 2 : 0;

    if (!skipAudioDevice)
    {
        juce::Logger::writeToLog("Initializing audio devices (inputs: " + juce::String(numInputChannels) + ", outputs: 2)...");
        auto err = deviceManager.initialiseWithDefaultDevices(numInputChannels, 2);
        juce::Logger::writeToLog("Audio devices initialized.");
        if (err.isNotEmpty())
        {
            juce::Logger::writeToLog("Error initializing audio devices: " + err);
            return false;
        }
    }

    juce::File pluginFile(pluginPath);
    if (!pluginFile.existsAsFile() && !pluginFile.isDirectory())
    {
        juce::Logger::writeToLog("Plugin file does not exist: " + pluginPath);
        return false;
    }

    juce::VST3PluginFormat vst3Format;

    juce::String fileOrId = pluginFile.getFullPathName();
    juce::Logger::writeToLog("Scanning: " + fileOrId);

    juce::OwnedArray<juce::PluginDescription> typesFound;

    if (!vst3Format.fileMightContainThisPluginType(fileOrId))
        juce::Logger::writeToLog("Warning: fileMightContainThisPluginType returned false.");

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

    const auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (auto& slot : midiInputSlots)
    {
        if (slot.sourceType != MidiInputSlot::SourceType::Device)
            continue;

        const auto deviceIndex = resolveRequestedIdToZeroBasedIndex(slot.requestedDeviceId);
        if (!juce::isPositiveAndBelow(deviceIndex, midiInputs.size()))
        {
            juce::Logger::writeToLog("midi_" + juce::String(slot.slotIndex)
                + " requested invalid device ID " + juce::String(slot.requestedDeviceId)
                + " (available MIDI devices: " + juce::String(midiInputs.size()) + ")");
            continue;
        }

        const auto& deviceInfo = midiInputs.getReference(deviceIndex);
        slot.deviceIdentifier = deviceInfo.identifier;
        if (!activeMidiDeviceIdentifiers.contains(slot.deviceIdentifier))
        {
            deviceManager.setMidiInputDeviceEnabled(slot.deviceIdentifier, true);
            deviceManager.addMidiInputDeviceCallback(slot.deviceIdentifier, this);
            activeMidiDeviceIdentifiers.add(slot.deviceIdentifier);
        }

        juce::Logger::writeToLog("midi_" + juce::String(slot.slotIndex) + " mapped to MIDI device #"
            + juce::String(slot.requestedDeviceId) + ": " + deviceInfo.name);
    }

    logRoutingSummary();

    if (!skipAudioDevice)
        deviceManager.addAudioCallback(this);

    return true;
}

void HostApp::shutdown()
{
    deviceManager.removeAudioCallback(this);

    for (const auto& identifier : activeMidiDeviceIdentifiers)
    {
        deviceManager.removeMidiInputDeviceCallback(identifier, this);
        deviceManager.setMidiInputDeviceEnabled(identifier, false);
    }
    activeMidiDeviceIdentifiers.clear();

    releasePlaybackState();
    pluginInstance.reset();
}

bool HostApp::runTest()
{
    if (pluginInstance == nullptr)
        return false;

    juce::Logger::writeToLog("--- Starting Test Mode ---");
    logRoutingSummary();

    // Drive processBlock directly with a self-contained local buffer.
    // No audio device involvement, no shared state with the live callback —
    // this is the original safe design that avoids any concurrency issues.
    const int numBlocksToProcess = 10;
    const int bufferSize = 512;
    const double sampleRate = 44100.0;

    pluginInstance->prepareToPlay(sampleRate, bufferSize);

    const int channels = juce::jmax(pluginInstance->getTotalNumInputChannels(),
                                     pluginInstance->getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer(juce::jmax(1, channels), bufferSize);
    juce::MidiBuffer midiBuffer;

    hostTransportPlayHead.update(configuredBpm, sampleRate, 0, true, false);
    ScopedPluginPlayHead scopedPlayHead(*pluginInstance, hostTransportPlayHead);

    for (int i = 0; i < numBlocksToProcess; ++i)
    {
        buffer.clear();
        hostTransportPlayHead.update(configuredBpm, sampleRate,
                                     (int64_t)i * bufferSize, true, false);
        pluginInstance->processBlock(buffer, midiBuffer);
    }

    pluginInstance->releaseResources();

    juce::Logger::writeToLog("Processed " + juce::String(numBlocksToProcess) + " blocks successfully.");
    return true;
}

void HostApp::parseConfig(const juce::String& configPath)
{
    audioInputSlots.clear();
    midiInputSlots.clear();
    hasConfiguredAudioSlots = false;
    hasConfiguredMidiSlots = false;
    hasLoadedMidiFileSequence = false;

    juce::File configFile;
    if (configPath.isNotEmpty())
    {
        configFile = juce::File(configPath);
        juce::Logger::writeToLog("Using config path from command line: " + configFile.getFullPathName());
    }
    else
    {
        configFile = juce::File::getCurrentWorkingDirectory().getChildFile("minihost_config.json");
        juce::Logger::writeToLog("Using default config path in working directory: " + configFile.getFullPathName());
    }

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

    auto resolvePathFromConfig = [&](juce::String path) -> juce::String
    {
        if (path.isNotEmpty() && !juce::File::isAbsolutePath(path))
            path = configFile.getParentDirectory().getChildFile(path).getFullPathName();
        return path;
    };

    if (auto* obj = config.getDynamicObject())
    {
        std::map<int, juce::var> audioSlotsFromConfig;
        std::map<int, juce::var> midiSlotsFromConfig;

        const auto& properties = obj->getProperties();
        for (int i = 0; i < properties.size(); ++i)
        {
            const auto propertyName = properties.getName(i);
            const auto propertyValue = properties.getValueAt(i);

            int slotIndex = 0;
            if (tryParseIndexedSlotKey(propertyName, "audio", slotIndex))
                audioSlotsFromConfig[slotIndex] = propertyValue;
            else if (tryParseIndexedSlotKey(propertyName, "midi", slotIndex))
                midiSlotsFromConfig[slotIndex] = propertyValue;
        }

        // Backwards compatibility with legacy keys.
        if (audioSlotsFromConfig.empty() && obj->hasProperty("audio_file"))
            audioSlotsFromConfig[1] = obj->getProperty("audio_file");
        if (midiSlotsFromConfig.empty() && obj->hasProperty("midi_file"))
            midiSlotsFromConfig[1] = obj->getProperty("midi_file");

        for (const auto& [slotIndex, slotValue] : audioSlotsFromConfig)
        {
            AudioInputSlot slot;
            slot.slotIndex = slotIndex;
            slot.pluginInputChannel = slotIndex - 1;

            int requestedId = 0;
            if (tryParseIntegerId(slotValue, requestedId))
            {
                if (requestedId < 0)
                {
                    juce::Logger::writeToLog("Ignoring audio_" + juce::String(slotIndex)
                        + ": device ID must be >= 0.");
                    continue;
                }

                slot.sourceType = AudioInputSlot::SourceType::DeviceChannel;
                slot.requestedDeviceId = requestedId;
                slot.deviceInputChannel = resolveRequestedIdToZeroBasedIndex(requestedId);
                juce::Logger::writeToLog("Config loaded audio_" + juce::String(slotIndex)
                    + " as device input channel ID " + juce::String(requestedId));
            }
            else if (slotValue.isString())
            {
                slot.sourceType = AudioInputSlot::SourceType::File;
                slot.filePath = resolvePathFromConfig(slotValue.toString());
                juce::Logger::writeToLog("Config loaded audio_" + juce::String(slotIndex)
                    + " as file: " + slot.filePath);
            }
            else
            {
                juce::Logger::writeToLog("Ignoring audio_" + juce::String(slotIndex)
                    + ": expected string file path or integer device/channel ID.");
                continue;
            }

            audioInputSlots.push_back(std::move(slot));
        }

        for (const auto& [slotIndex, slotValue] : midiSlotsFromConfig)
        {
            MidiInputSlot slot;
            slot.slotIndex = slotIndex;

            int requestedId = 0;
            if (tryParseIntegerId(slotValue, requestedId))
            {
                if (requestedId < 0)
                {
                    juce::Logger::writeToLog("Ignoring midi_" + juce::String(slotIndex)
                        + ": device ID must be >= 0.");
                    continue;
                }

                slot.sourceType = MidiInputSlot::SourceType::Device;
                slot.requestedDeviceId = requestedId;
                juce::Logger::writeToLog("Config loaded midi_" + juce::String(slotIndex)
                    + " as MIDI device ID " + juce::String(requestedId));
            }
            else if (slotValue.isString())
            {
                slot.sourceType = MidiInputSlot::SourceType::File;
                slot.filePath = resolvePathFromConfig(slotValue.toString());
                juce::Logger::writeToLog("Config loaded midi_" + juce::String(slotIndex)
                    + " as file: " + slot.filePath);
            }
            else
            {
                juce::Logger::writeToLog("Ignoring midi_" + juce::String(slotIndex)
                    + ": expected string MIDI file path or integer MIDI device ID.");
                continue;
            }

            midiInputSlots.push_back(std::move(slot));
        }

        hasConfiguredAudioSlots = !audioInputSlots.empty();
        hasConfiguredMidiSlots = !midiInputSlots.empty();

        if (obj->hasProperty("bpm"))
        {
            auto bpm = static_cast<double>(obj->getProperty("bpm"));
            if (bpm > 0.0)
            {
                configuredBpm = bpm;
                juce::Logger::writeToLog("Config loaded bpm: " + juce::String(configuredBpm, 2));
            }
            else
            {
                juce::Logger::writeToLog("Ignoring invalid bpm in config (must be > 0).");
            }
        }
    }
}

void HostApp::applyBpmToMidiSequence(double bpm)
{
    if (midiSequence.getNumEvents() == 0 || bpm <= 0.0)
        return;

    // JUCE conversion defaults to 120 BPM when no tempo map is present.
    // Scaling by 120/bpm provides a simple user BPM control.
    const double timeScale = 120.0 / bpm;
    if (std::abs(timeScale - 1.0) < 1.0e-9)
        return;

    for (int i = 0; i < midiSequence.getNumEvents(); ++i)
    {
        if (auto* event = midiSequence.getEventPointer(i))
            event->message.setTimeStamp(event->message.getTimeStamp() * timeScale);
    }
}

void HostApp::logRoutingSummary()
{
    if (pluginInstance == nullptr)
        return;

    juce::Logger::writeToLog("--- Routing Summary ---");
    juce::Logger::writeToLog("Plugin: " + pluginInstance->getName());
    juce::Logger::writeToLog("Plugin total input channels: " + juce::String(pluginInstance->getTotalNumInputChannels()));
    juce::Logger::writeToLog("Plugin total output channels: " + juce::String(pluginInstance->getTotalNumOutputChannels()));

    const auto numInputBuses = pluginInstance->getBusCount(true);
    const auto numOutputBuses = pluginInstance->getBusCount(false);
    juce::Logger::writeToLog("Plugin input buses: " + juce::String(numInputBuses));
    for (int i = 0; i < numInputBuses; ++i)
    {
        if (const auto* bus = pluginInstance->getBus(true, i))
        {
            juce::Logger::writeToLog("  In bus " + juce::String(i + 1) + ": "
                + bus->getName() + " (" + juce::String(bus->getNumberOfChannels()) + " ch)");
        }
    }

    juce::Logger::writeToLog("Plugin output buses: " + juce::String(numOutputBuses));
    for (int i = 0; i < numOutputBuses; ++i)
    {
        if (const auto* bus = pluginInstance->getBus(false, i))
        {
            juce::Logger::writeToLog("  Out bus " + juce::String(i + 1) + ": "
                + bus->getName() + " (" + juce::String(bus->getNumberOfChannels()) + " ch)");
        }
    }

    if (const auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::Logger::writeToLog("Current audio device: " + device->getName()
            + " (active input channels: "
            + juce::String(device->getActiveInputChannels().countNumberOfSetBits())
            + ", active output channels: "
            + juce::String(device->getActiveOutputChannels().countNumberOfSetBits()) + ")");
    }

    const auto midiDevices = juce::MidiInput::getAvailableDevices();
    juce::Logger::writeToLog("Available MIDI input devices: " + juce::String(midiDevices.size()));
    for (int i = 0; i < midiDevices.size(); ++i)
        juce::Logger::writeToLog("  MIDI device ID " + juce::String(i + 1) + ": " + midiDevices.getReference(i).name);

    if (audioInputSlots.empty())
    {
        juce::Logger::writeToLog("Audio input slots: none configured (fallback sine wave enabled).");
    }
    else
    {
        juce::Logger::writeToLog("Audio input slots: " + juce::String(audioInputSlots.size()));
        for (const auto& slot : audioInputSlots)
        {
            if (slot.sourceType == AudioInputSlot::SourceType::File)
            {
                juce::Logger::writeToLog("  audio_" + juce::String(slot.slotIndex)
                    + " -> plugin input channel " + juce::String(slot.pluginInputChannel + 1)
                    + " from file: " + slot.filePath);
            }
            else
            {
                juce::Logger::writeToLog("  audio_" + juce::String(slot.slotIndex)
                    + " -> plugin input channel " + juce::String(slot.pluginInputChannel + 1)
                    + " from audio input channel ID " + juce::String(slot.requestedDeviceId));
            }
        }
    }

    if (midiInputSlots.empty())
    {
        juce::Logger::writeToLog("MIDI input slots: none configured (fallback MIDI pattern enabled).");
    }
    else
    {
        juce::Logger::writeToLog("MIDI input slots: " + juce::String(midiInputSlots.size()));
        for (const auto& slot : midiInputSlots)
        {
            if (slot.sourceType == MidiInputSlot::SourceType::File)
            {
                juce::Logger::writeToLog("  midi_" + juce::String(slot.slotIndex)
                    + " -> MIDI file: " + slot.filePath);
            }
            else
            {
                juce::Logger::writeToLog("  midi_" + juce::String(slot.slotIndex)
                    + " -> MIDI device ID " + juce::String(slot.requestedDeviceId));
            }
        }
    }
}

void HostApp::preparePlaybackState(double sampleRate, int blockSize)
{
    if (pluginInstance == nullptr)
        return;

    const auto processChannels = juce::jmax(pluginInstance->getTotalNumInputChannels(),
                                            pluginInstance->getTotalNumOutputChannels());

    pluginInstance->prepareToPlay(sampleRate, blockSize);
    internalBuffer.setSize(processChannels, blockSize);
    slotScratchBuffer.setSize(1, blockSize, false, false, true);

    playbackSamplePosition = 0;
    nextMidiEventIndex = 0;
    midiSequence.clear();
    hasLoadedMidiFileSequence = false;

    for (auto& slot : audioInputSlots)
    {
        slot.warnedInvalidChannel = false;
        slot.readerSource.reset();
        slot.transportSource.reset();

        if (slot.sourceType != AudioInputSlot::SourceType::File)
            continue;

        juce::File audioFile(slot.filePath);
        if (auto* reader = audioFormatManager.createReaderFor(audioFile))
        {
            slot.readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            slot.transportSource = std::make_unique<juce::AudioTransportSource>();
            slot.transportSource->setSource(slot.readerSource.get(), 0, nullptr, reader->sampleRate);
            slot.transportSource->prepareToPlay(blockSize, sampleRate);
            slot.transportSource->start();
            juce::Logger::writeToLog("Loaded audio file for audio_" + juce::String(slot.slotIndex)
                + ": " + slot.filePath);
        }
        else
        {
            juce::Logger::writeToLog("Failed to read audio file for audio_" + juce::String(slot.slotIndex)
                + ": " + slot.filePath);
        }
    }

    int loadedMidiFiles = 0;
    for (const auto& slot : midiInputSlots)
    {
        if (slot.sourceType != MidiInputSlot::SourceType::File)
            continue;

        juce::File midiFile(slot.filePath);
        juce::FileInputStream midiFileStream(midiFile);
        if (!midiFileStream.openedOk())
        {
            juce::Logger::writeToLog("Failed to read MIDI file for midi_" + juce::String(slot.slotIndex)
                + ": " + slot.filePath);
            continue;
        }

        juce::MidiFile parsedMidi;
        if (!parsedMidi.readFrom(midiFileStream))
        {
            juce::Logger::writeToLog("Failed to parse MIDI file for midi_" + juce::String(slot.slotIndex)
                + ": " + slot.filePath);
            continue;
        }

        parsedMidi.convertTimestampTicksToSeconds();
        for (int trackIndex = 0; trackIndex < parsedMidi.getNumTracks(); ++trackIndex)
            midiSequence.addSequence(*parsedMidi.getTrack(trackIndex), 0.0);

        ++loadedMidiFiles;
    }

    if (midiSequence.getNumEvents() > 0)
    {
        applyBpmToMidiSequence(configuredBpm);

        double firstPlayableEventTime = std::numeric_limits<double>::max();
        for (int i = 0; i < midiSequence.getNumEvents(); ++i)
        {
            if (auto* event = midiSequence.getEventPointer(i); event != nullptr && !event->message.isMetaEvent())
                firstPlayableEventTime = std::min(firstPlayableEventTime, event->message.getTimeStamp());
        }

        if (firstPlayableEventTime != std::numeric_limits<double>::max() && firstPlayableEventTime > 0.0)
        {
            for (int i = 0; i < midiSequence.getNumEvents(); ++i)
            {
                if (auto* event = midiSequence.getEventPointer(i))
                    event->message.setTimeStamp(juce::jmax(0.0, event->message.getTimeStamp() - firstPlayableEventTime));
            }
        }

        midiSequence.updateMatchedPairs();
        hasLoadedMidiFileSequence = true;
        juce::Logger::writeToLog("Loaded " + juce::String(loadedMidiFiles)
            + " MIDI file slot(s) for playback (BPM: " + juce::String(configuredBpm, 2) + ").");
    }

    midiCollector.reset(sampleRate);

    const double secondsPerBeat = 60.0 / configuredBpm;
    fallbackNoteLengthSamples = static_cast<int64_t>(secondsPerBeat * sampleRate);
    fallbackRestLengthSamples = static_cast<int64_t>(secondsPerBeat * sampleRate);
    fallbackMidiSampleCounter = 0;
    fallbackMidiState = FallbackMidiState::NoteOn;
    sinePhase = 0.0;
}

void HostApp::releasePlaybackState()
{
    for (auto& slot : audioInputSlots)
    {
        if (slot.transportSource != nullptr)
        {
            slot.transportSource->stop();
            slot.transportSource->releaseResources();
        }
        slot.transportSource.reset();
        slot.readerSource.reset();
    }
}

void HostApp::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (pluginInstance == nullptr || device == nullptr)
        return;

    preparePlaybackState(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void HostApp::audioDeviceStopped()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();

    releasePlaybackState();
}

void HostApp::handleIncomingMidiMessage(juce::MidiInput* source,
                                        const juce::MidiMessage& message)
{
    juce::ignoreUnused(source);
    midiCollector.addMessageToQueue(message);
}

void HostApp::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                int numInputChannels,
                                                float* const* outputChannelData,
                                                int numOutputChannels,
                                                int numSamples,
                                                const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    if (pluginInstance == nullptr)
    {
        for (int i = 0; i < numOutputChannels; ++i)
        {
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
        }
        return;
    }

    renderNextBlock(inputChannelData, numInputChannels, numSamples);

    for (int i = 0; i < numOutputChannels; ++i)
    {
        if (outputChannelData[i] == nullptr)
            continue;

        if (i < internalBuffer.getNumChannels())
            juce::FloatVectorOperations::copy(outputChannelData[i], internalBuffer.getReadPointer(i), numSamples);
        else
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
    }
}

void HostApp::renderNextBlock(const float* const* inputChannelData,
                              int numInputChannels,
                              int numSamples)
{
    const auto processChannels = juce::jmax(pluginInstance->getTotalNumInputChannels(),
                                            pluginInstance->getTotalNumOutputChannels());
    internalBuffer.setSize(processChannels, numSamples, false, false, true);
    internalBuffer.clear();
    internalMidiBuffer.clear();

    if (hasConfiguredAudioSlots)
    {
        slotScratchBuffer.setSize(1, numSamples, false, false, true);

        for (auto& slot : audioInputSlots)
        {
            if (!juce::isPositiveAndBelow(slot.pluginInputChannel, internalBuffer.getNumChannels()))
                continue;

            if (slot.sourceType == AudioInputSlot::SourceType::File)
            {
                if (slot.transportSource != nullptr && slot.transportSource->isPlaying())
                {
                    slotScratchBuffer.clear();
                    juce::AudioSourceChannelInfo info(&slotScratchBuffer, 0, numSamples);
                    slot.transportSource->getNextAudioBlock(info);
                    internalBuffer.copyFrom(slot.pluginInputChannel, 0, slotScratchBuffer, 0, 0, numSamples);

                    if (slot.transportSource->hasStreamFinished())
                    {
                        if (shouldLoop)
                        {
                            slot.transportSource->setPosition(0.0);
                            slot.transportSource->start();
                        }
                        else
                        {
                            slot.transportSource->stop();
                        }
                    }
                }
            }
            else
            {
                if (juce::isPositiveAndBelow(slot.deviceInputChannel, numInputChannels)
                    && inputChannelData != nullptr
                    && inputChannelData[slot.deviceInputChannel] != nullptr)
                {
                    juce::FloatVectorOperations::copy(internalBuffer.getWritePointer(slot.pluginInputChannel),
                                                      inputChannelData[slot.deviceInputChannel],
                                                      numSamples);
                }
                else if (!slot.warnedInvalidChannel)
                {
                    juce::Logger::writeToLog("audio_" + juce::String(slot.slotIndex)
                        + " could not read input channel ID " + juce::String(slot.requestedDeviceId)
                        + " from current audio device.");
                    slot.warnedInvalidChannel = true;
                }
            }
        }
    }
    else
    {
        const double sampleRate = pluginInstance->getSampleRate() > 0.0 ? pluginInstance->getSampleRate() : 44100.0;
        const double phaseIncrement = juce::MathConstants<double>::twoPi * sineFrequency / sampleRate;
        const int numChannels = juce::jmax(1, pluginInstance->getTotalNumInputChannels());
        for (int s = 0; s < numSamples; ++s)
        {
            const float sample = 0.5f * static_cast<float>(std::sin(sinePhase));
            for (int ch = 0; ch < numChannels && ch < internalBuffer.getNumChannels(); ++ch)
                internalBuffer.setSample(ch, s, sample);

            sinePhase += phaseIncrement;
            if (sinePhase >= juce::MathConstants<double>::twoPi)
                sinePhase -= juce::MathConstants<double>::twoPi;
        }
    }

    if (!activeMidiDeviceIdentifiers.isEmpty())
        midiCollector.removeNextBlockOfMessages(internalMidiBuffer, numSamples);

    if (hasLoadedMidiFileSequence)
    {
        const auto sampleRate = pluginInstance->getSampleRate() > 0.0 ? pluginInstance->getSampleRate() : 44100.0;
        const double timeNow = playbackSamplePosition / sampleRate;
        const double nextTime = timeNow + (numSamples / sampleRate);

        while (nextMidiEventIndex < midiSequence.getNumEvents())
        {
            auto* event = midiSequence.getEventPointer(nextMidiEventIndex);
            const double eventTime = event->message.getTimeStamp();

            if (eventTime >= timeNow && eventTime < nextTime)
            {
                const int samplePos = juce::jlimit(0, numSamples - 1, static_cast<int>((eventTime - timeNow) * sampleRate));
                internalMidiBuffer.addEvent(event->message, samplePos);
                ++nextMidiEventIndex;
            }
            else if (eventTime >= nextTime)
            {
                break;
            }
            else
            {
                ++nextMidiEventIndex;
            }
        }

        if (nextMidiEventIndex >= midiSequence.getNumEvents() && shouldLoop)
        {
            nextMidiEventIndex = 0;
            playbackSamplePosition = 0;
        }
    }
    else if (!hasConfiguredMidiSlots)
    {
        // Fallback MIDI: note-on -> hold -> note-off -> rest -> repeat.
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

    const auto blockStartSample = playbackSamplePosition;
    const auto sampleRate = pluginInstance->getSampleRate() > 0.0 ? pluginInstance->getSampleRate() : 44100.0;
    hostTransportPlayHead.update(configuredBpm, sampleRate, blockStartSample, true, shouldLoop);
    ScopedPluginPlayHead scopedPlayHead(*pluginInstance, hostTransportPlayHead);
    pluginInstance->processBlock(internalBuffer, internalMidiBuffer);

    playbackSamplePosition += numSamples;
}
