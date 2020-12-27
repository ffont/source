/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   22nd April 2020).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "../Utility/Helpers/jucer_VersionInfo.h"

class DownloadAndInstallThread;

class LatestVersionCheckerAndUpdater   : public DeletedAtShutdown,
                                         private Thread
{
public:
    LatestVersionCheckerAndUpdater();
    ~LatestVersionCheckerAndUpdater() override;

    void checkForNewVersion (bool showAlerts);

    //==============================================================================
    JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL (LatestVersionCheckerAndUpdater)

private:
    //==============================================================================
    void run() override;
    void askUserAboutNewVersion (const String&, const String&, const VersionInfo::Asset&);
    void askUserForLocationToDownload (const VersionInfo::Asset&);
    void downloadAndInstall (const VersionInfo::Asset&, const File&);

    //==============================================================================
    bool showAlertWindows = false;

    std::unique_ptr<DownloadAndInstallThread> installer;
    std::unique_ptr<Component> dialogWindow;
};
