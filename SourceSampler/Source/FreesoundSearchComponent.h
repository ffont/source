/*
  ==============================================================================

    FreesoundSearchComponent.h
    Created: 10 Sep 2019 5:44:51pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"
#include "api_key.h"


class ResultsTableComponent : public Component,
                              public TableListBoxModel
{
public:
    ResultsTableComponent() {
    
        table.setModel(this);
    
        addAndMakeVisible (table);
        
        table.getHeader().addColumn("Name", 1, 200, 50, 1000, (TableHeaderComponent::visible | TableHeaderComponent::resizable ));
        table.getHeader().addColumn("Author", 2, 100, 50, 1000, (TableHeaderComponent::visible | TableHeaderComponent::resizable ));
        table.getHeader().addColumn("License", 3, 400, 50, 1000, (TableHeaderComponent::visible | TableHeaderComponent::resizable ));
    }
    
    int getNumRows() override {
        return (int)data.size();
    }
    
    void paintRowBackground (Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override
    {
        auto alternateColour = getLookAndFeel().findColour (ListBox::backgroundColourId)
        .interpolatedWith (getLookAndFeel().findColour (ListBox::textColourId), 0.03f);
        g.fillAll (alternateColour);
    }
    
    void paintCell (Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override
    {
        g.setColour (getLookAndFeel().findColour (ListBox::textColourId));
        auto text = data[rowNumber].getReference(columnId - 1);
        g.drawText (text, 2, 0, width - 4, height, Justification::centredLeft, true);
        g.setColour (getLookAndFeel().findColour (ListBox::backgroundColourId));
        g.fillRect (width - 1, 0, 1, height);
    }
    
    void addRowData(const StringArray itemsStringArray) {
        data.push_back(itemsStringArray);
    }
    
    void updateContent() {
        table.updateContent();
        table.repaint();
    }
    
    void clearItems() {
        data.clear();
    }
    
    void resized() override {
        table.setBounds(getLocalBounds());
    }

	void cellClicked(int rowNumber, int columnId, const MouseEvent &) {
		processor->addToMidiBuffer(rowNumber * 8);
	}

	void setProcessor(SourceSamplerAudioProcessor* p)
	{
		processor = p;
		if (p->isArrayNotEmpty()) {
			data = p->getData();
			updateContent();
		}
	}

	std::vector<StringArray> getData() {
		return data;
	}
    
private:
    TableListBox table;
    std::vector<StringArray> data;
	SourceSamplerAudioProcessor * processor;
};


class FreesoundSearchComponent: public Component,
                                public ActionListener,
                                public Button::Listener
{
public:
    
    FreesoundSearchComponent ()
    {
        searchInput.setText("", dontSendNotification);
        searchInput.setColour (Label::backgroundColourId, getLookAndFeel().findColour (ResizableWindow::backgroundColourId).brighter());
        searchInput.setEditable (true);
        addAndMakeVisible (searchInput);
        
        searchButton.addListener (this);
        searchButton.setButtonText("Go!");
        addAndMakeVisible (searchButton);
        
        addAndMakeVisible (searchResults);
    }
    
    ~FreesoundSearchComponent ()
    {
        processor->removeActionListener(this);  // Stop receivng messages from processor
    }
    
    void setProcessor (SourceSamplerAudioProcessor* p)
    {
        processor = p;
		searchResults.setProcessor(p);
		searchInput.setText(p->getQuery(), dontSendNotification);
        
        processor->addActionListener(this);  // Receive messages from processor
    }
    
    void paint (Graphics& g) override
    {
    }
    
    void resized () override
    {
        float unitMargin = 10;
        float searchButtonWidth = 100;
        float inputHeight = 20;
        
        searchInput.setBounds(0, 0, getWidth() - unitMargin - searchButtonWidth, inputHeight);
        searchButton.setBounds(getWidth()  - searchButtonWidth, 0, searchButtonWidth, inputHeight);
        searchResults.setBounds(0, inputHeight + unitMargin, getWidth(), getHeight() - (inputHeight + unitMargin));
    }
    
    void buttonClicked (Button* button) override
    {
        if (button == &searchButton)
        {
            processor->makeQueryAndLoadSounds(searchInput.getText(true));
        }
    }
    
    void updateSoundsTable ()
    {
        searchResults.clearItems();
        std::vector<juce::StringArray> soundsArray = processor->getData();
        for (int i=0; i<soundsArray.size(); i++){
            searchResults.addRowData(soundsArray[i]);
        }
        searchResults.updateContent();
    }
    
    void actionListenerCallback (const String &message) override
    {
        if (message.startsWith("SHOULD_UPDATE_SOUNDS_TABLE"))
        {
            updateSoundsTable();
        }
    }
    
private:
    
    SourceSamplerAudioProcessor* processor;
    
    Label searchInput;
    TextButton searchButton;
    ResultsTableComponent searchResults;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreesoundSearchComponent);
};
