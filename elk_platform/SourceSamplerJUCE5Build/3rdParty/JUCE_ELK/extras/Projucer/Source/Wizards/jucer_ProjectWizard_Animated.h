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


//==============================================================================
struct AnimatedAppWizard   : public NewProjectWizard
{
    AnimatedAppWizard()  {}

    String getName() const override         { return TRANS("Animated Application"); }
    String getDescription() const override  { return TRANS("Creates an application which draws an animated graphical display."); }
    const char* getIcon() const override    { return BinaryData::wizard_AnimatedApp_svg; }

    StringArray getFileCreationOptions() override
    {
        return { "Create header and implementation files",
                 "Create header file only" };
    }

    Result processResultsFromSetupItems (WizardComp& setupComp) override
    {
        createCppFile = false;

        switch (setupComp.getFileCreationComboID())
        {
            case 0:     createCppFile = true;  break;
            case 1:     break;
            default:    jassertfalse; break;
        }

        return Result::ok();
    }

    bool initialiseProject (Project& project) override
    {
        createSourceFolder();

        File mainCppFile    = getSourceFilesFolder().getChildFile ("Main.cpp");
        File contentCompCpp = getSourceFilesFolder().getChildFile ("MainComponent.cpp");
        File contentCompH   = contentCompCpp.withFileExtension (".h");
        String contentCompName = "MainComponent";

        project.setProjectType (ProjectType_GUIApp::getTypeName());

        Project::Item sourceGroup (createSourceGroup (project));

        setExecutableNameForAllTargets (project, File::createLegalFileName (appTitle));

        auto juceHeaderInclude = CodeHelpers::createIncludePathIncludeStatement (Project::getJuceSourceHFilename());
        auto appHeaders = juceHeaderInclude + newLine + CodeHelpers::createIncludeStatement (contentCompH, mainCppFile);

        // create main window
        String windowH = project.getFileTemplate (createCppFile ? "jucer_AnimatedComponentTemplate_h"
                                                                : "jucer_AnimatedComponentSimpleTemplate_h")
                            .replace ("%%include_juce%%", juceHeaderInclude)
                            .replace ("%%content_component_class%%", contentCompName, false);

        if (! FileHelpers::overwriteFileWithNewDataIfDifferent (contentCompH, windowH))
            failedFiles.add (contentCompH.getFullPathName());

        sourceGroup.addFileAtIndex (contentCompH, -1, false);

        if (createCppFile)
        {
            String windowCpp = project.getFileTemplate ("jucer_AnimatedComponentTemplate_cpp")
                                  .replace ("%%include_juce%%", juceHeaderInclude)
                                  .replace ("%%include_corresponding_header%%", CodeHelpers::createIncludeStatement (contentCompH, contentCompCpp), false)
                                  .replace ("%%content_component_class%%", contentCompName, false);

            if (! FileHelpers::overwriteFileWithNewDataIfDifferent (contentCompCpp, windowCpp))
                failedFiles.add (contentCompCpp.getFullPathName());

            sourceGroup.addFileAtIndex (contentCompCpp, -1, true);
        }

        // create main cpp
        String mainCpp = project.getFileTemplate ("jucer_MainTemplate_SimpleWindow_cpp")
                            .replace ("%%app_headers%%", appHeaders, false)
                            .replace ("%%app_class_name%%", CodeHelpers::makeValidIdentifier (appTitle + "Application", false, true, false), false)
                            .replace ("%%content_component_class%%", contentCompName, false)
                            .replace ("%%allow_more_than_one_instance%%", "true", false);

        if (! FileHelpers::overwriteFileWithNewDataIfDifferent (mainCppFile, mainCpp))
            failedFiles.add (mainCppFile.getFullPathName());

        sourceGroup.addFileAtIndex (mainCppFile,-1,  true);

        return true;
    }

private:
    bool createCppFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnimatedAppWizard)
};