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
    
    void setSoundsToDownload(std::vector<std::pair<File, String>> _soundTargetLocationsAndUrlsToDownload){
        soundTargetLocationsAndUrlsToDownload = _soundTargetLocationsAndUrlsToDownload;
    }
    
    void run(){
        downloadAllSounds();
    }
    
    void downloadAllSounds(){
        for (int i=0; i<soundTargetLocationsAndUrlsToDownload.size();i++){
            File location = soundTargetLocationsAndUrlsToDownload[i].first;
            String url = soundTargetLocationsAndUrlsToDownload[i].second;
            if (!location.exists()){  // Dont' re-download if file already exists
                std::unique_ptr<URL::DownloadTask> downloadTask = URL(url).downloadToFile(location, "", this);
                downloadTasks.push_back(std::move(downloadTask));
            } else {
                // If sound already downloaded, show trigger action
                String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + location.getFullPathName();
                sendActionMessage(actionMessage);
            }
        }
    }
    
    void finished(URL::DownloadTask *task, bool success){
        String actionMessage = String(ACTION_FINISHED_DOWNLOADING_SOUND) + ":" + task->getTargetLocation().getFullPathName();
        sendActionMessage(actionMessage);
    }
    
    void progress (URL::DownloadTask *task, int64 bytesDownloaded, int64 totalLength){
        String serializedParameters = task->getTargetLocation().getFullPathName() + SERIALIZATION_SEPARATOR + (String)(100*bytesDownloaded/totalLength) + SERIALIZATION_SEPARATOR;
        String actionMessage = String(ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS) + ":" + serializedParameters;
        sendActionMessage(actionMessage);
    }
    
private:
    std::vector<std::pair<File, String>> soundTargetLocationsAndUrlsToDownload = {};
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    bool allFinished = false;
    
};
