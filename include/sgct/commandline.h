/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__COMMANDLINE__H__
#define __SGCT__COMMANDLINE__H__

#include <sgct/messagehandler.h>
#include <sgct/settings.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgct {

struct Configuration {
    std::optional<std::string> configFilename;
    std::optional<bool> isServer;
    std::optional<std::string> logPath;
    std::optional<MessageHandler::Level> logLevel;
    std::optional<bool> showHelpText;
    std::optional<int> nodeId;
    std::optional<bool> firmSync;
    std::optional<bool> ignoreSync;
    std::optional<bool> fxaa;
    std::optional<int> msaaSamples;
    std::optional<Settings::CaptureFormat> captureFormat;
    std::optional<int> nCaptureThreads;
    std::optional<bool> checkOpenGL;
    std::optional<bool> checkFBOs;
};

/**
 * Command line parameters are used to load a configuration file and settings. The
 * arguments that were successfully extracted are removed from the vector that is passed
 * into this function.
 *
 * \param arg The list of arguments that should be processed by this function
 * \return The filled struct with commandline options
 */
Configuration parseArguments(std::vector<std::string>& arg);

/// Returns the text providing information about the available commandline options
std::string_view getHelpMessage();

} // namespace sgct

#endif // __SGCT__COMMANDLINE__H__