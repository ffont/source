/*
  ==============================================================================

    TestMain.cpp - runs all juce::UnitTest tests in the "Source" category

    Usage: SourceSamplerTests [test name]

  ==============================================================================
*/

#include <JuceHeader.h>


int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;  // Message manager is needed (state changes happen in the message thread)

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    if (argc > 1)
        runner.runTestsWithName (argv[1]);  // NOTE: assumes test names are unique
    else
        runner.runTestsInCategory ("Source");

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult (i)->failures;

    std::cout << (numFailures == 0 ? "ALL TESTS PASSED" : juce::String (numFailures) + " FAILURE(S)") << std::endl;
    return numFailures == 0 ? 0 : 1;
}
