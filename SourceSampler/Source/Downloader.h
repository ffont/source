/*
  ==============================================================================

    Downloader.h
    Created: 21 Sep 2020 3:34:51pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "defines.h"


class Downloader: public ActionBroadcaster,
                  public Thread
{
public:
    
    Downloader(): Thread ("DownloaderThread"){}
    
    ~Downloader(){
        for (int i = 0; i < downloadTasks.size(); i++) {
            downloadTasks.at(i).reset();
        }
    }
    
    void setBaseDownloadLocation(File _baseDownloadLocation){
       baseDownloadLocation = _baseDownloadLocation;
   }
    
    void setSoundsToDownload(std::vector<std::pair<String, String>> _soundIdsUrlsToDownload){
        soundIDsUrlsToDownload = _soundIdsUrlsToDownload;
    }
    
    void run(){
        downloadAllSounds();
        sendActionMessage(ACTION_FINISHED_DOWNLOADING_SOUNDS);
    }
    
    void downloadAllSounds(){
        if (baseDownloadLocation.isDirectory()){
            for (int i=0; i<soundIDsUrlsToDownload.size();i++){
                String soundID = soundIDsUrlsToDownload[i].first;
                String url = soundIDsUrlsToDownload[i].second;
                File location = baseDownloadLocation.getChildFile(soundID).withFileExtension("ogg");
                if (!location.exists()){  // Dont' re-download if file already exists
                    std::unique_ptr<URL::DownloadTask> downloadTask = URL(url).downloadToFile(location, "");
                    downloadTasks.push_back(std::move(downloadTask));
                }
            }
            
            int64 startedWaitingTime = Time::getCurrentTime().toMilliseconds();
            while (!allFinished){
                allFinished = true;
                for (int i=0; i<downloadTasks.size(); i++){
                    if (!downloadTasks[i]->isFinished()){
                        allFinished = false;
                    }
                }
                if (Time::getCurrentTime().toMilliseconds() - startedWaitingTime > MAX_DOWNLOAD_WAITING_TIME_MS){
                    // If more than 10 seconds, mark all as finished
                    allFinished = true;
                }
            }
        }
    }

private:
    std::vector<std::pair<String, String>> soundIDsUrlsToDownload = {};
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    bool allFinished = false;
    File baseDownloadLocation;
    
};
