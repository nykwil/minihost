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
        // Set up a simple file logger on the Desktop
        juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("minihost.log");
        if (logFile.exists()) logFile.deleteFile();
        fileLogger = new juce::FileLogger(logFile, "Log started");
        
        juce::Logger::setCurrentLogger(fileLogger);
        juce::Logger::writeToLog("Logger initialized. Getting command line args...");

        auto args = juce::JUCEApplication::getCommandLineParameterArray();
        if (args.isEmpty())
        {
            juce::Logger::writeToLog("Error: No plugin path provided. Usage: minihost [--test] <path_to_vst3>");
            juce::JUCEApplication::quit();
            return;
        }

        bool runTestingMode = false;
        juce::String pluginPath;

        for (const auto& arg : args)
        {
            if (arg == "--test")
                runTestingMode = true;
            else
                pluginPath = arg; // We'll assume the last non-flag argument is the path
        }
        
        if (pluginPath.isEmpty())
        {
            juce::Logger::writeToLog("Error: No plugin path provided.");
            juce::JUCEApplication::quit();
            return;
        }

        juce::Logger::writeToLog("Plugin path parsed: " + pluginPath);

        hostApp = std::make_unique<HostApp>();
        juce::Logger::writeToLog("HostApp created. Abstracting audio/midi devices...");
        
        if (!hostApp->initialise(pluginPath))
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
