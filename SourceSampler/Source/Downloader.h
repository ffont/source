/*
  ==============================================================================

    Downloader.h
    Created: 21 Sep 2020 3:34:51pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "defines.h"


class Downloader: public ActionBroadcaster,
                  public URL::DownloadTask::Listener,
                  public Thread
{
public:
    
    Downloader(): Thread ("DownloaderThread"){}
    
    ~Downloader(){
        for (int i = 0; i < downloadTasks.size(); i++) {
            downloadTasks.at(i).reset();
        }
    }
    
    void setSoundsToDownload(std::vector<std::tuple<File, String, String>> _soundTargetLocationsAndUrlsToDownload){
        soundTargetLocationsAndUrlsToDownload = _soundTargetLocationsAndUrlsToDownload;
    }
    
    void run(){
        downloadAllSounds();
    }
    
    void downloadAllSounds(){
        for (int i=0; i<soundTargetLocationsAndUrlsToDownload.size();i++){
            File baseLocation = std::get<0>(soundTargetLocationsAndUrlsToDownload[i]);
            String soundID = std::get<1>(soundTargetLocationsAndUrlsToDownload[i]);
            String previewUrl = std::get<2>(soundTargetLocationsAndUrlsToDownload[i]);
            File location = baseLocation.getChildFile(soundID).withFileExtension("ogg");
            if (!location.exists()){  // Dont' re-download if file already exists
                std::unique_ptr<URL::DownloadTask> downloadTask = URL(previewUrl).downloadToFile(location, "", this);
                downloadTasks.push_back(std::move(downloadTask));
            } else {
                // If sound already downloaded, show trigger action
                String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + soundID;
                sendActionMessage(actionMessage);
            }
        }
    }
    
    void finished(URL::DownloadTask *task, bool success){
        StringArray tokens;
        tokens.addTokens (task->getTargetLocation().getFullPathName(), "/", "");
        String filename = tokens[tokens.size() - 1];
        tokens.clear();
        tokens.addTokens (filename, ".ogg", "");
        String soundID = tokens[0];
        String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + soundID;
        sendActionMessage(actionMessage);
    }
    
    void progress (URL::DownloadTask *task, int64 bytesDownloaded, int64 totalLength){
        StringArray tokens;
        tokens.addTokens (task->getTargetLocation().getFullPathName(), "/", "");
        String filename = tokens[tokens.size() - 1];
        tokens.clear();
        tokens.addTokens (filename, ".ogg", "");
        String soundID = tokens[0];
        String serializedParameters = soundID + SERIALIZATION_SEPARATOR + (String)(100*bytesDownloaded/totalLength) + SERIALIZATION_SEPARATOR;
        String actionMessage = String(ACTION_DOWNLOADING_SOUND_PROGRESS) + ":" + serializedParameters;
        sendActionMessage(actionMessage);
    }
    
private:
    std::vector<std::tuple<File, String, String>> soundTargetLocationsAndUrlsToDownload = {};
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    bool allFinished = false;
    
};
