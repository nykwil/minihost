#include <algorithm>
#include <JuceHeader.h>
#include "HostApp.h"

class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow(juce::AudioProcessorEditor* editor)
        : DocumentWindow("Plugin Interface",
                         juce::Desktop::getInstance().getDefaultLookAndFeel()
                             .findColour(juce::ResizableWindow::backgroundColourId),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);

        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
        editor->grabKeyboardFocus();
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

class MiniHostApplication : public juce::JUCEApplication
{
public:
    MiniHostApplication() {}

    const juce::String getApplicationName() override       { return "minihost"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);

        // Set up a simple file logger on the Desktop
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("minihost.log");
        if (logFile.exists()) logFile.deleteFile();
        fileLogger = new juce::FileLogger(logFile, "Log started");
        
        juce::Logger::setCurrentLogger(fileLogger);
        juce::Logger::writeToLog("Logger initialized. Getting command line args...");

        auto args = juce::JUCEApplication::getCommandLineParameterArray();
        if (args.isEmpty())
        {
            juce::Logger::writeToLog("Error: No plugin path provided. Usage: minihost [--test] [--config <path_to_json>] [--bpm <value>] <path_to_vst3>");
            juce::JUCEApplication::quit();
            return;
        }

        bool runTestingMode = false;
        juce::String pluginPath;
        juce::String configPath;
        double bpmOverride = 0.0;

        for (int i = 0; i < args.size(); ++i)
        {
            const auto& arg = args[i];

            if (arg == "--test")
            {
                runTestingMode = true;
            }
            else if (arg == "--config")
            {
                if (i + 1 < args.size())
                    configPath = args[++i];
                else
                {
                    juce::Logger::writeToLog("Error: --config requires a path argument.");
                    juce::JUCEApplication::quit();
                    return;
                }
            }
            else if (arg.startsWith("--config="))
            {
                configPath = arg.fromFirstOccurrenceOf("=", false, false);
            }
            else if (arg == "--bpm")
            {
                if (i + 1 < args.size())
                {
                    bpmOverride = args[++i].getDoubleValue();
                    if (bpmOverride <= 0.0)
                    {
                        juce::Logger::writeToLog("Error: --bpm requires a numeric value > 0.");
                        juce::JUCEApplication::quit();
                        return;
                    }
                }
                else
                {
                    juce::Logger::writeToLog("Error: --bpm requires a value.");
                    juce::JUCEApplication::quit();
                    return;
                }
            }
            else if (arg.startsWith("--bpm="))
            {
                bpmOverride = arg.fromFirstOccurrenceOf("=", false, false).getDoubleValue();
                if (bpmOverride <= 0.0)
                {
                    juce::Logger::writeToLog("Error: --bpm requires a numeric value > 0.");
                    juce::JUCEApplication::quit();
                    return;
                }
            }
            else
            {
                pluginPath = arg; // We'll assume the last non-flag argument is the path
            }
        }
        
        if (pluginPath.isEmpty())
        {
            juce::Logger::writeToLog("Error: No plugin path provided.");
            juce::JUCEApplication::quit();
            return;
        }

        juce::Logger::writeToLog("Plugin path parsed: " + pluginPath);
        if (configPath.isNotEmpty())
            juce::Logger::writeToLog("Config path parsed: " + configPath);
        if (bpmOverride > 0.0)
            juce::Logger::writeToLog("BPM override parsed: " + juce::String(bpmOverride, 2));

        hostApp = std::make_unique<HostApp>();
        juce::Logger::writeToLog("HostApp created. Abstracting audio/midi devices...");
        
        if (!hostApp->initialise(pluginPath, configPath, bpmOverride))
        {
            juce::Logger::writeToLog("Failed to initialize HostApp with plugin: " + pluginPath);
            juce::JUCEApplication::quit();
            return;
        }

        if (runTestingMode)
        {
            hostApp->setLooping(false);
            juce::Logger::writeToLog("Testing mode enabled.");
            bool testSuccess = hostApp->runTest();
            if (testSuccess)
                juce::Logger::writeToLog("Testing completed successfully.");
            else
                juce::Logger::writeToLog("Error during test mode.");
            
            juce::JUCEApplication::quit();
            return;
        }
        else
        {
            hostApp->setLooping(true);
            juce::Logger::writeToLog("Host is running. Plugin window opening...");
            if (auto* pluginInst = hostApp->getPluginInstance())
            {
                if (pluginInst->hasEditor())
                {
                    mainWindow = std::make_unique<PluginWindow>(pluginInst->createEditor());
                }
                else
                {
                    juce::Logger::writeToLog("This plugin doesn't have a UI editor.");
                }
            }
        }
    }

    void shutdown() override
    {
        mainWindow.reset();
        hostApp.reset();
        juce::Logger::writeToLog("Host shut down.");
        juce::Logger::setCurrentLogger(nullptr);
        delete fileLogger;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    juce::Logger* fileLogger = nullptr;
    std::unique_ptr<HostApp> hostApp;
    std::unique_ptr<PluginWindow> mainWindow;
};

START_JUCE_APPLICATION (MiniHostApplication)
