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
#include <algorithm>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>


class DownloadPool
{
public:
    static constexpr int maxSimultaneousDownloads = 6;
    /** How long to wait for the server to respond (and between received data). The Freesound CDN can take close to a
        minute to start sending files that are not in its cache. */
    static constexpr int responseTimeoutMs = 120000;
    /** Downloads that fail because of the server or the network (timeouts, 5xx/429 responses, dropped connections)
        are retried, waiting a bit longer each time. Other errors (e.g. 404) are not retried. */
    static constexpr int maxAttempts = 3;
    static constexpr int retryDelayMs = 3000;

    struct Request
    {
        juce::URL url;
        juce::String extraHeaders;
        juce::File target;
    };

    enum class Result { succeeded, failedTemporarily, failedPermanently, cancelled };

    /** Called from a pool thread: bytes downloaded and total (or -1 if unknown). */
    using ProgressCallback = std::function<void (const juce::File& target, juce::int64 downloaded, juce::int64 total)>;
    /** Called from a pool thread when the download ends (not called if it was cancelled). On success the file is
        already at its target location. */
    using FinishedCallback = std::function<void (const juce::File& target, bool succeeded)>;

    /** Shared flag used to cancel a group of downloads (e.g. all downloads of a sound when it is deleted). */
    using CancelFlag = std::shared_ptr<std::atomic<bool>>;
    static CancelFlag makeCancelFlag() { return std::make_shared<std::atomic<bool>> (false); }

    DownloadPool()
        : pool (juce::ThreadPoolOptions().withThreadName ("SourceDownloads").withNumberOfThreads (maxSimultaneousDownloads)) {}

    ~DownloadPool()
    {
        // Abort the connections in progress (they could otherwise keep the threads waiting for the server)
        shuttingDown = true;
        abortActiveDownloads (nullptr);
        pool.removeAllJobs (true, 10000);
    }

    /** Adds a download to the queue. Returns immediately. */
    void download (Request request, CancelFlag cancelled, ProgressCallback onProgress, FinishedCallback onFinished)
    {
        pool.addJob (new DownloadJob (*this, std::move (request), std::move (cancelled), std::move (onProgress), std::move (onFinished)), true);
    }

    /** Cancels a group of downloads without waiting: queued downloads are skipped and the connections of the ones in
        progress are aborted. Their finished callbacks are not called. */
    void cancel (const CancelFlag& flag)
    {
        flag->store (true);
        abortActiveDownloads (flag.get());
    }

    int getNumPendingDownloads() const { return pool.getNumJobs(); }

    /** Base delay between attempts (the n-th retry waits n times this). */
    void setRetryDelayMs (int newDelayMs) { retryDelay = newDelayMs; }

    /** Downloads a URL to a file in the calling thread, retrying when the failure might be temporary. */
    Result downloadWithRetries (const Request& request, const CancelFlag& flag, const std::function<bool()>& shouldStop,
                                const ProgressCallback& onProgress)
    {
        Result result = Result::failedTemporarily;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt)
        {
            result = downloadOnce (request, flag, shouldStop, onProgress);
            if (result != Result::failedTemporarily || attempt == maxAttempts)
                break;
            DBG ("Download of " << request.url.toString (false) << " failed (attempt " << attempt << "), retrying");
            // Wait before retrying (in small steps, so cancelling is not delayed)
            const auto retryTime = juce::Time::getMillisecondCounter() + (juce::uint32) (retryDelay.load() * attempt);
            while (juce::Time::getMillisecondCounter() < retryTime)
            {
                if (shouldStop())
                    return Result::cancelled;
                juce::Thread::sleep (50);
            }
        }
        return result;
    }

    /** Downloads a URL to a file in the calling thread (single attempt). */
    Result downloadOnce (const Request& request, const CancelFlag& flag, const std::function<bool()>& shouldStop,
                         const ProgressCallback& onProgress)
    {
        juce::WebInputStream stream (request.url, false);
        stream.withConnectionTimeout (responseTimeoutMs).withNumRedirectsToFollow (5);
        if (request.extraHeaders.isNotEmpty())
            stream.withExtraHeaders (request.extraHeaders);

        // Register the stream so it can be aborted from other threads while waiting for the server
        ActiveDownload activeDownload (*this, stream, flag.get());
        if (shouldStop() || shuttingDown)
            return Result::cancelled;

        const bool connected = stream.connect (nullptr);
        if (shouldStop())
            return Result::cancelled;
        const int status = stream.getStatusCode();
        if (! connected || stream.isError() || status >= 400)
        {
            DBG ("Download of " << request.url.toString (false) << " failed with status " << status);
            return isTemporaryFailure (status) ? Result::failedTemporarily : Result::failedPermanently;
        }

        // Download to a temporary file first, so a partial download is never taken as a complete one
        auto partialFile = request.target.getSiblingFile (request.target.getFileName() + ".part");
        request.target.getParentDirectory().createDirectory();
        partialFile.deleteFile();
        {
            juce::FileOutputStream out (partialFile);
            if (! out.openedOk())
                return Result::failedPermanently;
            const auto total = stream.getTotalLength();
            juce::HeapBlock<char> buffer (32768);
            juce::int64 downloaded = 0;
            auto lastProgressTime = juce::Time::getMillisecondCounter();
            while (! stream.isExhausted())
            {
                if (shouldStop())
                    break;
                const int numRead = stream.read (buffer.getData(), 32768);
                if (numRead <= 0 || stream.isError())
                    break;
                if (! out.write (buffer.getData(), (size_t) numRead))
                    break;
                downloaded += numRead;
                const auto now = juce::Time::getMillisecondCounter();
                if (onProgress != nullptr && now - lastProgressTime > 100)
                {
                    lastProgressTime = now;
                    onProgress (request.target, downloaded, total);
                }
            }
            out.flush();
            if (shouldStop())
            {
                partialFile.deleteFile();
                return Result::cancelled;
            }
            if (downloaded == 0 || (total > 0 && downloaded != total) || stream.isError() || out.getStatus().failed())
            {
                partialFile.deleteFile();
                return Result::failedTemporarily;  // Connection dropped (or the file could not be written)
            }
            if (onProgress != nullptr)
                onProgress (request.target, downloaded, total > 0 ? total : downloaded);
        }
        request.target.deleteFile();
        return partialFile.moveFileTo (request.target) ? Result::succeeded : Result::failedPermanently;
    }

    /** True for failures that might work if retried: no response (timeout, network error) or server errors. */
    static bool isTemporaryFailure (int statusCode)
    {
        return statusCode == 0 || statusCode == 408 || statusCode == 429 || statusCode >= 500;
    }

private:
    /** Registers a stream in progress (so it can be aborted) for the lifetime of this object. */
    struct ActiveDownload
    {
        ActiveDownload (DownloadPool& p, juce::WebInputStream& s, std::atomic<bool>* f) : owner (p), stream (s)
        {
            const juce::ScopedLock sl (owner.activeDownloadsLock);
            owner.activeDownloads.push_back ({ &stream, f });
        }
        ~ActiveDownload()
        {
            const juce::ScopedLock sl (owner.activeDownloadsLock);
            auto& v = owner.activeDownloads;
            v.erase (std::remove_if (v.begin(), v.end(), [this] (const auto& d) { return d.first == &stream; }), v.end());
        }
        DownloadPool& owner;
        juce::WebInputStream& stream;
    };

    /** Aborts the connections of the downloads in the given group (or of all downloads if flag is nullptr). */
    void abortActiveDownloads (std::atomic<bool>* flag)
    {
        const juce::ScopedLock sl (activeDownloadsLock);
        for (auto& [stream, streamFlag] : activeDownloads)
            if (flag == nullptr || streamFlag == flag)
                stream->cancel();
    }

    class DownloadJob : public juce::ThreadPoolJob
    {
    public:
        DownloadJob (DownloadPool& p, Request r, CancelFlag c, ProgressCallback pr, FinishedCallback f)
            : juce::ThreadPoolJob ("SourceDownload"), owner (p), request (std::move (r)), cancelled (std::move (c)),
              onProgress (std::move (pr)), onFinished (std::move (f)) {}

        JobStatus runJob() override
        {
            auto shouldStop = [this] { return shouldExit() || cancelled->load() || owner.shuttingDown.load(); };
            if (shouldStop())
                return jobHasFinished;
            const auto result = owner.downloadWithRetries (request, cancelled, shouldStop, onProgress);
            if (result != Result::cancelled && ! shouldStop() && onFinished != nullptr)
                onFinished (request.target, result == Result::succeeded);
            return jobHasFinished;
        }

    private:
        DownloadPool& owner;
        Request request;
        CancelFlag cancelled;
        ProgressCallback onProgress;
        FinishedCallback onFinished;
    };

    juce::CriticalSection activeDownloadsLock;
    std::vector<std::pair<juce::WebInputStream*, std::atomic<bool>*>> activeDownloads;
    std::atomic<bool> shuttingDown { false };
    std::atomic<int> retryDelay { retryDelayMs };
    juce::ThreadPool pool;  // Last member: destroyed (and its threads stopped) first

    JUCE_DECLARE_NON_COPYABLE (DownloadPool)
};
