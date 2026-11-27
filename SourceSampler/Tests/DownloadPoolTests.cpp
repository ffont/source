/*
  ==============================================================================

    DownloadPoolTests.cpp - tests for the background downloader, using a small
    local HTTP server (no internet needed)

  ==============================================================================
*/

#include <JuceHeader.h>
#include "DownloadPool.h"
#include "TestUtils.h"


/** Minimal HTTP server: answers each request with the next scripted response (the last one is repeated). A response
    with status 0 accepts the connection but never answers. */
class ScriptedHttpServer : private juce::Thread
{
public:
    struct Response { int status; juce::String body; };

    explicit ScriptedHttpServer (std::vector<Response> r) : juce::Thread ("ScriptedHttpServer"), responses (std::move (r))
    {
        for (port = 47800; port < 47900; ++port)
            if (socket.createListener (port, "127.0.0.1"))
                break;
        startThread();
    }

    ~ScriptedHttpServer() override
    {
        signalThreadShouldExit();
        socket.close();
        stopThread (5000);
    }

    juce::URL getUrl (const juce::String& path) const { return juce::URL ("http://127.0.0.1:" + juce::String (port) + path); }
    int getNumRequests() const { return numRequests.load(); }

private:
    void run() override
    {
        std::vector<std::unique_ptr<juce::StreamingSocket>> hangingConnections;
        while (! threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> client (socket.waitForNextConnection());
            if (client == nullptr)
                continue;
            // Read the request headers
            juce::String request;
            char buffer[1024];
            while (! request.contains ("\r\n\r\n") && client->waitUntilReady (true, 2000) == 1)
            {
                const int n = client->read (buffer, sizeof (buffer), false);
                if (n <= 0)
                    break;
                request += juce::String::fromUTF8 (buffer, n);
            }
            const auto& response = responses[(size_t) juce::jmin ((int) responses.size() - 1, numRequests.load())];
            numRequests += 1;
            if (response.status == 0)
            {
                hangingConnections.push_back (std::move (client));  // Never answer
                continue;
            }
            const auto body = response.body.toStdString();
            const auto header = "HTTP/1.1 " + juce::String (response.status) + " Status\r\nContent-Type: application/octet-stream\r\n"
                                + "Content-Length: " + juce::String ((int) body.size()) + "\r\nConnection: close\r\n\r\n";
            client->write (header.toRawUTF8(), (int) header.getNumBytesAsUTF8());
            client->write (body.data(), (int) body.size());
            client->close();
        }
    }

    juce::StreamingSocket socket;
    int port = 0;
    std::vector<Response> responses;
    std::atomic<int> numRequests { 0 };
};


class DownloadPoolTests : public juce::UnitTest
{
public:
    DownloadPoolTests() : juce::UnitTest ("Download pool", "Source") {}

    void runTest() override
    {
        TestUtils::TemporaryFolder tmp;
        const juce::String fileContents = juce::String::repeatedString ("0123456789", 5000);

        beginTest ("Server errors are retried until the download succeeds");
        {
            ScriptedHttpServer server ({ { 503, "busy" }, { 502, "bad gateway" }, { 200, fileContents } });
            DownloadPool pool;
            pool.setRetryDelayMs (50);
            auto target = tmp.folder.getChildFile ("retried.ogg");
            auto flag = DownloadPool::makeCancelFlag();
            const auto result = pool.downloadWithRetries ({ server.getUrl ("/retried.ogg"), {}, target }, flag, [] { return false; }, nullptr);
            expect (result == DownloadPool::Result::succeeded);
            expectEquals (server.getNumRequests(), 3);
            expectEquals (target.loadFileAsString(), fileContents);
            expect (! target.getSiblingFile ("retried.ogg.part").exists(), "Partial file left behind");
        }

        beginTest ("Missing files are not retried");
        {
            ScriptedHttpServer server ({ { 404, "not found" } });
            DownloadPool pool;
            pool.setRetryDelayMs (50);
            auto target = tmp.folder.getChildFile ("missing.ogg");
            const auto result = pool.downloadWithRetries ({ server.getUrl ("/missing.ogg"), {}, target }, DownloadPool::makeCancelFlag(), [] { return false; }, nullptr);
            expect (result == DownloadPool::Result::failedPermanently);
            expectEquals (server.getNumRequests(), 1);
            expect (! target.exists());
        }

        beginTest ("Server errors fail after the maximum number of attempts");
        {
            ScriptedHttpServer server ({ { 503, "busy" } });
            DownloadPool pool;
            pool.setRetryDelayMs (50);
            std::atomic<int> numFinished { 0 }, numSucceeded { 0 };
            pool.download ({ server.getUrl ("/busy.ogg"), {}, tmp.folder.getChildFile ("busy.ogg") }, DownloadPool::makeCancelFlag(), nullptr,
                           [&] (const juce::File&, bool succeeded) { numFinished += 1; numSucceeded += succeeded ? 1 : 0; });
            expect (TestUtils::runMessageLoopUntil ([&] { return numFinished.load() == 1; }, 10000));
            expectEquals (numSucceeded.load(), 0);
            expectEquals (server.getNumRequests(), DownloadPool::maxAttempts);
        }

        beginTest ("Cancelling aborts downloads waiting for the server");
        {
            ScriptedHttpServer server ({ { 0, {} } });  // Never answers
            DownloadPool pool;
            std::atomic<int> numFinished { 0 };
            auto flag = DownloadPool::makeCancelFlag();
            pool.download ({ server.getUrl ("/hang.ogg"), {}, tmp.folder.getChildFile ("hang.ogg") }, flag, nullptr,
                           [&] (const juce::File&, bool) { numFinished += 1; });
            expect (TestUtils::runMessageLoopUntil ([&] { return server.getNumRequests() == 1; }, 5000), "Request not sent");
            const auto start = juce::Time::getMillisecondCounterHiRes();
            pool.cancel (flag);
            expect (TestUtils::runMessageLoopUntil ([&] { return pool.getNumPendingDownloads() == 0; }, 5000), "Download was not aborted");
            expectLessThan (juce::Time::getMillisecondCounterHiRes() - start, 3000.0);
            expectEquals (numFinished.load(), 0, "Cancelled downloads don't report");
        }

        beginTest ("Destroying the pool aborts downloads waiting for the server");
        {
            ScriptedHttpServer server ({ { 0, {} } });
            const auto start = juce::Time::getMillisecondCounterHiRes();
            {
                DownloadPool pool;
                pool.download ({ server.getUrl ("/hang.ogg"), {}, tmp.folder.getChildFile ("hang2.ogg") }, DownloadPool::makeCancelFlag(), nullptr, nullptr);
                expect (TestUtils::runMessageLoopUntil ([&] { return server.getNumRequests() == 1; }, 5000), "Request not sent");
            }
            expectLessThan (juce::Time::getMillisecondCounterHiRes() - start, 5000.0);
        }

        beginTest ("Temporary failures");
        for (int status : { 0, 408, 429, 500, 502, 503, 504 })
            expect (DownloadPool::isTemporaryFailure (status), juce::String (status));
        for (int status : { 400, 401, 403, 404 })
            expect (! DownloadPool::isTemporaryFailure (status), juce::String (status));
    }
};

static DownloadPoolTests downloadPoolTests;
