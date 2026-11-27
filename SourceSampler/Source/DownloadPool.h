/*
  ==============================================================================

    DownloadPool.h

    Downloads files in a small pool of background threads. JUCE's
    URL::downloadToFile opens the HTTP connection synchronously in the calling
    thread (on desktop), so calling it from the message thread for every sample
    of a preset blocks the UI. It also starts one thread per download. This pool
    never blocks the caller and limits the number of simultaneous downloads.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>


class DownloadPool
{
public:
    static constexpr int maxSimultaneousDownloads = 6;
    static constexpr int connectionTimeoutMs = 15000;

    struct Request
    {
        juce::URL url;
        juce::String extraHeaders;
        juce::File target;
    };

    /** Called from a pool thread: bytes downloaded and total (or -1 if unknown). */
    using ProgressCallback = std::function<void (const juce::File& target, juce::int64 downloaded, juce::int64 total)>;
    /** Called from a pool thread when the download ends (not called if it was cancelled). On success the file is
        already at its target location. */
    using FinishedCallback = std::function<void (const juce::File& target, bool succeeded)>;

    /** Shared flag used to cancel a group of downloads (e.g. all downloads of a sound when it is deleted). Queued
        downloads of a cancelled group are skipped and running ones stop after the current chunk. */
    using CancelFlag = std::shared_ptr<std::atomic<bool>>;
    static CancelFlag makeCancelFlag() { return std::make_shared<std::atomic<bool>> (false); }

    DownloadPool()
        : pool (juce::ThreadPoolOptions().withThreadName ("SourceDownloads").withNumberOfThreads (maxSimultaneousDownloads)) {}

    ~DownloadPool()
    {
        pool.removeAllJobs (true, connectionTimeoutMs + 1000);
    }

    /** Adds a download to the queue. Returns immediately. */
    void download (Request request, CancelFlag cancelled, ProgressCallback onProgress, FinishedCallback onFinished)
    {
        pool.addJob (new DownloadJob (std::move (request), std::move (cancelled), std::move (onProgress), std::move (onFinished)), true);
    }

    int getNumPendingDownloads() const { return pool.getNumJobs(); }

    /** Downloads a URL to a file in the calling thread (used by the jobs, and by tests). */
    static bool downloadNow (const Request& request, std::function<bool()> shouldStop, const ProgressCallback& onProgress)
    {
        juce::WebInputStream stream (request.url, false);
        stream.withConnectionTimeout (connectionTimeoutMs).withNumRedirectsToFollow (5);
        if (request.extraHeaders.isNotEmpty())
            stream.withExtraHeaders (request.extraHeaders);
        if (shouldStop() || ! stream.connect (nullptr) || stream.isError())
            return false;
        const int status = stream.getStatusCode();
        if (status >= 400)
            return false;

        // Download to a temporary file first, so a partial download is never taken as a complete one
        auto partialFile = request.target.getSiblingFile (request.target.getFileName() + ".part");
        request.target.getParentDirectory().createDirectory();
        partialFile.deleteFile();
        {
            juce::FileOutputStream out (partialFile);
            if (! out.openedOk())
                return false;
            const auto total = stream.getTotalLength();
            juce::HeapBlock<char> buffer (32768);
            juce::int64 downloaded = 0;
            auto lastProgressTime = juce::Time::getMillisecondCounter();
            while (! stream.isExhausted())
            {
                if (shouldStop())
                {
                    out.flush();
                    partialFile.deleteFile();
                    return false;
                }
                const int numRead = stream.read (buffer.getData(), 32768);
                if (numRead <= 0 || stream.isError())
                    break;
                if (! out.write (buffer.getData(), (size_t) numRead))
                    return false;
                downloaded += numRead;
                const auto now = juce::Time::getMillisecondCounter();
                if (onProgress != nullptr && now - lastProgressTime > 100)
                {
                    lastProgressTime = now;
                    onProgress (request.target, downloaded, total);
                }
            }
            out.flush();
            if (downloaded == 0 || (total > 0 && downloaded != total) || stream.isError())
            {
                partialFile.deleteFile();
                return false;
            }
            if (onProgress != nullptr)
                onProgress (request.target, downloaded, total > 0 ? total : downloaded);
        }
        request.target.deleteFile();
        return partialFile.moveFileTo (request.target);
    }

private:
    class DownloadJob : public juce::ThreadPoolJob
    {
    public:
        DownloadJob (Request r, CancelFlag c, ProgressCallback p, FinishedCallback f)
            : juce::ThreadPoolJob ("SourceDownload"), request (std::move (r)), cancelled (std::move (c)),
              onProgress (std::move (p)), onFinished (std::move (f)) {}

        JobStatus runJob() override
        {
            auto shouldStop = [this] { return shouldExit() || cancelled->load(); };
            if (shouldStop())
                return jobHasFinished;
            const bool succeeded = downloadNow (request, shouldStop, onProgress);
            if (! shouldStop() && onFinished != nullptr)
                onFinished (request.target, succeeded);
            return jobHasFinished;
        }

    private:
        Request request;
        CancelFlag cancelled;
        ProgressCallback onProgress;
        FinishedCallback onFinished;
    };

    juce::ThreadPool pool;

    JUCE_DECLARE_NON_COPYABLE (DownloadPool)
};
