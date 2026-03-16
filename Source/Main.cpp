#include <cstdio>
#include <JuceHeader.h>
#include "HostApp.h"

namespace
{
class CompositeLogger : public juce::Logger
{
public:
    CompositeLogger(bool logToConsoleIn, std::unique_ptr<juce::FileLogger> fileLoggerIn)
        : logToConsole(logToConsoleIn), fileLogger(std::move(fileLoggerIn))
    {
    }

    void logMessage(const juce::String& message) override
    {
        if (logToConsole)
        {
            const auto lower = message.toLowerCase();
            const bool isErrorLike = lower.startsWith("error")
                || lower.contains("failed")
                || lower.contains("invalid");
            auto* stream = isErrorLike ? stderr : stdout;
            std::fputs(message.toRawUTF8(), stream);
            std::fputc('\n', stream);
            std::fflush(stream);
        }

        if (fileLogger != nullptr)
            fileLogger->logMessage(message);
    }

private:
    bool logToConsole = false;
    std::unique_ptr<juce::FileLogger> fileLogger;
};

juce::File getDefaultConfigFile(const juce::String& configPath)
{
    if (configPath.isNotEmpty())
        return juce::File(configPath);

    return juce::File::getCurrentWorkingDirectory().getChildFile("minihost_config.json");
}

juce::String getConfiguredLogPath(const juce::File& configFile)
{
    if (!configFile.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(configFile.loadFileAsString());
    if (!parsed.isObject())
        return {};

    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty("log_path"))
            return obj->getProperty("log_path").toString();
    }

    return {};
}

juce::File resolveLogFile(const juce::String& logPath, const juce::File& configFile)
{
    if (logPath.isEmpty())
        return {};

    if (juce::File::isAbsolutePath(logPath))
        return juce::File(logPath);

    return configFile.getParentDirectory().getChildFile(logPath);
}
}

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

        auto args = juce::JUCEApplication::getCommandLineParameterArray();
        bool runTestingMode = false;
        juce::String pluginPath;
        juce::String configPath;
        double bpmOverride = 0.0;
        juce::String argumentError;

        if (args.isEmpty())
        {
            argumentError = "Error: No plugin path provided. Usage: minihost [--test] [--config <path_to_json>] [--bpm <value>] <path_to_vst3>";
        }
        else for (int i = 0; i < args.size(); ++i)
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
                    argumentError = "Error: --config requires a path argument.";
                    break;
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
                        argumentError = "Error: --bpm requires a numeric value > 0.";
                        break;
                    }
                }
                else
                {
                    argumentError = "Error: --bpm requires a value.";
                    break;
                }
            }
            else if (arg.startsWith("--bpm="))
            {
                bpmOverride = arg.fromFirstOccurrenceOf("=", false, false).getDoubleValue();
                if (bpmOverride <= 0.0)
                {
                    argumentError = "Error: --bpm requires a numeric value > 0.";
                    break;
                }
            }
            else
            {
                pluginPath = arg; // We'll assume the last non-flag argument is the path
            }
        }

        if (argumentError.isEmpty() && pluginPath.isEmpty())
            argumentError = "Error: No plugin path provided.";

        const auto configFileForLogging = getDefaultConfigFile(configPath);
        const auto configuredLogPath = getConfiguredLogPath(configFileForLogging);
        juce::File resolvedLogFile = resolveLogFile(configuredLogPath, configFileForLogging);

        if (resolvedLogFile == juce::File())
        {
            // Keep GUI mode behavior stable when no explicit log_path is set.
            if (!runTestingMode)
                resolvedLogFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("minihost.log");
        }

        std::unique_ptr<juce::FileLogger> fileLogger;
        if (resolvedLogFile != juce::File())
        {
            if (resolvedLogFile.existsAsFile())
                resolvedLogFile.deleteFile();
            fileLogger = std::make_unique<juce::FileLogger>(resolvedLogFile, "Log started");
        }

        activeLogger = std::make_unique<CompositeLogger>(runTestingMode, std::move(fileLogger));
        juce::Logger::setCurrentLogger(activeLogger.get());
        juce::Logger::writeToLog("Logger initialized. Getting command line args...");
        if (resolvedLogFile != juce::File())
            juce::Logger::writeToLog("Log file path: " + resolvedLogFile.getFullPathName());
        else
            juce::Logger::writeToLog("Log file path: none (console-only).");

        if (argumentError.isNotEmpty())
        {
            juce::Logger::writeToLog(argumentError);
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
        activeLogger.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<juce::Logger> activeLogger;
    std::unique_ptr<HostApp> hostApp;
    std::unique_ptr<PluginWindow> mainWindow;
};

START_JUCE_APPLICATION (MiniHostApplication)
