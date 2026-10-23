#pragma once
#include <JuceHeader.h>
#include "FunctionalList.h"
#include "FeatureList.h"
#include "ExtractionEngine.h"
#include "MyTheme.h"

class MainComponent  : public AudioAppComponent {
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:

    Label batchOutputLabel;
    Label liveOutputLabel;

    SectionPanel inputPanel{ "Input",    juce::Colour::fromString("#FF2D9CFF") };
    SectionPanel analysisPanel{ "Analysis", juce::Colour::fromString("#FFA700FF") };
    SectionPanel meteringPanel{ "Waveform", juce::Colour::fromString("#FFFF7B00") };
    SectionPanel outputPanel{ "Output",   juce::Colour::fromString("#FF42C800") };

    ModernLookAndFeel customLF;

    CircleIconButton settingsBtn{ "Settings", BinaryData::settings_svg, BinaryData::settings_svgSize };
    CircleIconButton monitorBtn{ "Monitor", BinaryData::headphones_svg, BinaryData::headphones_svgSize };


    AudioVisualiserComponent waveViewer{ 1 }; //per ora un canale

    void updateInterfaceState();

    std::vector<Feature*> getActiveFeatures();
    std::vector<Functional*> getActiveFunctionals();

    Label csvLabel;
    FuncList funcList;
    FeatList featList;
    MyFeatureExtractor extractor{ deviceManager };
    TextEditor csvNameEditor;
    Label csvSelectFolderLabel;
    Label csvNameLabel;

    //sample rate
    Slider rateSlider;
    Label rateLabel;


    //midi  
    ComboBox midiOutputList; //midi out list for user selection
    Label midiOutputListLabel;
    ToggleButton midiCheck;
    Label midiTitleLabel;

    //osc 
    Label oscIPLabel, oscPortLabel;
    TextEditor oscIPEditor, oscPortEditor;
    ToggleButton oscCheck;
    Label oscTitleLabel;

    //csv
    TextButton csvPathButton;
    void pathButtonClicked();
    std::unique_ptr<FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};