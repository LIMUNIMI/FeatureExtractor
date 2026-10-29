#include "MainComponent.h"
#include "TemporalFeatures.h"
#include "SpectralFeatures.h"

//TODO: usare valuetreestate
//TODO: tasto refresh midi
//TODO: output volume sotto rate
//TODO: disattivamento ui
//TODO: finestra avviso se non selezioni functional o feature
//TODO: fix bug canali live input
//TODO: macro file a parte

MainComponent::MainComponent() {

    setLookAndFeel(&customLF);

    addAndMakeVisible(&settingsBtn);
    settingsBtn.onClick = [this] {
        auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager, 1, 2, 1, 2, false, false, false, false);
        selector->setSize(500, 450);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector);
        options.dialogTitle = "Audio Settings";
        options.componentToCentreAround = this;
        options.launchAsync();
        };

    addAndMakeVisible(&monitorBtn);
    monitorBtn.setClickingTogglesState(true);
    monitorBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromString("#FF2D9CFF"));
    monitorBtn.onClick = [this] {
        extractor.setMonitorMode(monitorBtn.getToggleState());
        };

    juce::Colour darkBg = juce::Colour::fromString("#FF1A1A1E");
    juce::Colour panelBg = juce::Colour::fromString("#FF2A2A30");
    juce::Colour outlineColor = juce::Colour::fromString("#FF404048");
    juce::Colour accentBlue = juce::Colour::fromString("#FF2D9CFF");

    auto& lf = getLookAndFeel();
    lf.setColour(juce::ResizableWindow::backgroundColourId, darkBg);
    lf.setColour(juce::TextEditor::backgroundColourId, panelBg);
    lf.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    lf.setColour(juce::TextEditor::outlineColourId, outlineColor);
    lf.setColour(juce::TextEditor::focusedOutlineColourId, accentBlue);
    lf.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    lf.setColour(juce::ComboBox::backgroundColourId, panelBg);
    lf.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    lf.setColour(juce::ComboBox::outlineColourId, outlineColor);
    lf.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    lf.setColour(juce::ListBox::backgroundColourId, panelBg);
    lf.setColour(juce::ListBox::outlineColourId, outlineColor);
    lf.setColour(juce::ScrollBar::thumbColourId, outlineColor.brighter(0.2f));
    lf.setColour(juce::ScrollBar::backgroundColourId, panelBg);
    lf.setColour(juce::ScrollBar::trackColourId, panelBg);
    lf.setColour(juce::Label::textColourId, juce::Colours::white);
    lf.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    lf.setColour(juce::ToggleButton::tickColourId, accentBlue);
    lf.setColour(juce::ToggleButton::tickDisabledColourId, outlineColor);
    lf.setColour(juce::TextButton::buttonColourId, panelBg);
    lf.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    lf.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    lf.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    lf.setColour(juce::Slider::trackColourId, outlineColor);
    lf.setColour(juce::Slider::backgroundColourId, darkBg);

    addAndMakeVisible(&inputPanel);
    addAndMakeVisible(&analysisPanel);
    addAndMakeVisible(&meteringPanel);
    addAndMakeVisible(&outputPanel);

    addAndMakeVisible(waveViewer);
    waveViewer.setOpaque(false);
    waveViewer.setColours(juce::Colours::transparentBlack, panelBg.brighter(0.4f));
    waveViewer.setRepaintRate(60);
    waveViewer.setBufferSize(256);
    waveViewer.setSamplesPerBlock(256);

    addAndMakeVisible(rateSlider);
    rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    rateSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    rateSlider.setRange(5.0, 50.0, 1.0);
    rateSlider.setTextValueSuffix(" Hz");
    rateSlider.onValueChange = [this] {extractor.setUpdateRate(rateSlider.getValue()); };
    rateSlider.setValue(20.0);

    addAndMakeVisible(rateLabel);
    rateLabel.setText("Message Rate", juce::dontSendNotification);
    rateLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    rateLabel.setJustificationType(juce::Justification::centredLeft);

    extractor.getAudioPlayer().onLiveModeToggled = [this](bool isLive) {
        if (isLive) {
            extractor.setActiveFeatures(getActiveFeatures());
            extractor.prepareLiveFeatures();
        }
        extractor.setLiveMode(isLive); ///
        if (extractor.onStateChanged) extractor.onStateChanged();
        };

    extractor.getAudioPlayer().onPlaybackStarted = [this](double sampleRate, int blockSize) {
        extractor.setActiveFeatures(getActiveFeatures());
        extractor.prepareLiveFeatures();
        if (extractor.onStateChanged) extractor.onStateChanged();
        };

    extractor.onPrepareForBatch = [this] {
        if (extractor.onStateChanged) extractor.onStateChanged();
        extractor.setActiveFeatures(getActiveFeatures());
        extractor.setActiveFunctionals(getActiveFunctionals());
        };

    addAndMakeVisible(extractor.getAudioPlayer());

    addAndMakeVisible(&batchOutputLabel);
    batchOutputLabel.setText("Batch Output", juce::dontSendNotification);
    batchOutputLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    batchOutputLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(&liveOutputLabel);
    liveOutputLabel.setText("Live Output", juce::dontSendNotification);
    liveOutputLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    liveOutputLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(&csvLabel);
    csvLabel.setText("CSV", juce::dontSendNotification);
    csvLabel.setFont(juce::Font(15.0f, juce::Font::bold)); 

    addAndMakeVisible(&csvPathButton);
    csvPathButton.setButtonText(extractor.getCsvPath());
    csvPathButton.onClick = [this] {pathButtonClicked(); };

    addAndMakeVisible(&csvSelectFolderLabel);
    csvSelectFolderLabel.setText("Select CSV folder:", juce::dontSendNotification);

    addAndMakeVisible(csvNameEditor);
    csvNameEditor.setText(extractor.getCsvFileName());
    csvNameEditor.setJustification(juce::Justification::centred);
    csvNameEditor.onTextChange = [this] { extractor.setCsvFileName(csvNameEditor.getText()); };

    addAndMakeVisible(csvNameLabel);
    csvNameLabel.setText("CSV File Name:", juce::dontSendNotification);
    
    /// new
    addAndMakeVisible(exportLabel);
    exportLabel.setText("Export:", juce::dontSendNotification);

    addAndMakeVisible(exportModeList);
    exportModeList.addItem("Aggregate", 1);
    exportModeList.addItem("Frame", 2);
    exportModeList.addItem("Both", 3);
    exportModeList.setSelectedId(1, juce::dontSendNotification);
    exportModeList.onChange = [this] {
        updateBatch();
    };

    addAndMakeVisible(sepLabel);
    sepLabel.setText("Sep:", juce::dontSendNotification);


    addAndMakeVisible(sepEditor);
    sepEditor.setInputRestrictions(1);
    sepEditor.setText(";");
    sepEditor.setJustification(juce::Justification::centred);
    sepEditor.onTextChange = [this] {
        updateBatch();
    };

    addAndMakeVisible(decSepLabel);
    decSepLabel.setText("Dec:", juce::dontSendNotification);

    addAndMakeVisible(decSepEditor);
    decSepEditor.setInputRestrictions(1);
    decSepEditor.setText(",");
    decSepEditor.setJustification(juce::Justification::centred);
    decSepEditor.onTextChange = [this] {
        updateBatch();
    };

    addAndMakeVisible(precisionLabel);
    precisionLabel.setText("Prec:", juce::dontSendNotification);

    addAndMakeVisible(precisionList);

    for (int n = 1; n < 7; n++) {
        precisionList.addItem(juce::String(n), n);
    }

    precisionList.setSelectedId(4, juce::dontSendNotification);
    precisionList.onChange = [this] {
        updateBatch();
    };

    updateBatch();

    //

    extractor.onStateChanged = [this] { juce::MessageManager::callAsync([this] {
        updateInterfaceState();
        }); };

    addAndMakeVisible(midiTitleLabel);
    midiTitleLabel.setText("MIDI", juce::dontSendNotification);
    midiTitleLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(midiCheck);
    midiCheck.setButtonText("");
    midiCheck.onClick = [this] { extractor.setMidiEnabled(midiCheck.getToggleState()); };

    addAndMakeVisible(midiOutputListLabel);
    midiOutputListLabel.setText("MIDI Output:", juce::dontSendNotification);

    addAndMakeVisible(midiOutputList);
    midiOutputList.addItemList(extractor.getMidiOutput(), 1);
    midiOutputList.onChange = [this] {
        if (extractor.setMidiOutput(midiOutputList.getSelectedItemIndex())) {
            midiOutputList.setSelectedId(midiOutputList.getSelectedItemIndex() + 1, juce::dontSendNotification);
        }
        };
    if (midiOutputList.getSelectedId() == 0) { extractor.setMidiOutput(0); }

    addAndMakeVisible(oscTitleLabel);
    oscTitleLabel.setText("OSC", juce::dontSendNotification);
    oscTitleLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(oscCheck);
    oscCheck.setButtonText("");
    oscCheck.onClick = [this] { extractor.setOscEnabled(oscCheck.getToggleState()); };

    addAndMakeVisible(oscIPEditor);
    oscIPEditor.setText(extractor.getOscIP());
    oscIPEditor.setJustification(juce::Justification::centred);
    oscIPEditor.onTextChange = [this] {
        extractor.setOscIP(oscIPEditor.getText());
        extractor.connectOsc();
        };

    addAndMakeVisible(oscIPLabel);
    oscIPLabel.setText("IP Address:", juce::dontSendNotification);

    addAndMakeVisible(oscPortEditor);
    oscPortEditor.setText((juce::String)extractor.getOscPort());
    oscPortEditor.setJustification(juce::Justification::centred);
    oscPortEditor.onTextChange = [this] {
        extractor.setOscPort(oscPortEditor.getText().getIntValue());
        extractor.connectOsc();
        };

    addAndMakeVisible(oscPortLabel);
    oscPortLabel.setText("Port:", juce::dontSendNotification);

    addAndMakeVisible(&featList);
    addAndMakeVisible(&funcList);

    setSize(925, 850);

    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
            [&](bool granted) { setAudioChannels(granted ? 2 : 0, 2); });
    }
    else {
        setAudioChannels(2, 2);
    }
}

MainComponent::~MainComponent() {
    setLookAndFeel(nullptr);
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    extractor.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    extractor.getNextAudioBlock(bufferToFill);
    waveViewer.pushBuffer(bufferToFill);
    if (extractor.isLiveModeOn() && !extractor.isMonitorOn()) {
        bufferToFill.clearActiveBufferRegion();
    }
}

void MainComponent::updateBatch()
{
    switch (exportModeList.getSelectedId()) {
    case 1:
        extractor.setExport(Export::Aggregate);
        break;
    case 2:
        extractor.setExport(Export::Frame);
        break;
    case 3:
        extractor.setExport(Export::Both);
        break;
    }

    juce::String sep = sepEditor.getText();
    juce::String decSep = decSepEditor.getText();

    if (sep.isEmpty()) {
        sep = ";";
    }

    if (decSep.isEmpty()) {
        decSep = ",";
    }

    CsvFormat newFormat;
    newFormat.sep = sep;
    newFormat.decimalSep = decSep;
    newFormat.precision = precisionList.getSelectedId();

    extractor.setCsvFormat(newFormat);
}

void MainComponent::updateInterfaceState() {
    const bool shouldBeEnabled = extractor.canEditSettings();

    featList.setEnabled(shouldBeEnabled);
    funcList.setEnabled(shouldBeEnabled);
    oscIPEditor.setEnabled(shouldBeEnabled);
    oscPortEditor.setEnabled(shouldBeEnabled);
    midiCheck.setEnabled(shouldBeEnabled);
    oscCheck.setEnabled(shouldBeEnabled);
    midiOutputList.setEnabled(shouldBeEnabled);
    rateSlider.setEnabled(shouldBeEnabled);
    csvPathButton.setEnabled(shouldBeEnabled);
    extractor.getAudioPlayer().setInteractionEnabled(shouldBeEnabled);

    exportModeList.setEnabled(shouldBeEnabled);
    sepEditor.setEnabled(shouldBeEnabled);
    decSepEditor.setEnabled(shouldBeEnabled);
    precisionList.setEnabled(shouldBeEnabled);
}

std::vector<Feature*> MainComponent::getActiveFeatures() {
    std::vector<Feature*> active;
    auto& allFeatures = featList.getFeatures();
    for (int i = 0; i < allFeatures.size(); ++i) {
        if (featList.isRowSelected(i)) {
            active.push_back(allFeatures[i]);
        }
    }
    return active;
}

std::vector<Functional*> MainComponent::getActiveFunctionals() {
    std::vector<Functional*> active;
    auto& allFunctionals = funcList.getFunctionals();
    for (int i = 0; i < allFunctionals.size(); ++i) {
        if (funcList.isRowSelected(i)) {
            active.push_back(allFunctionals[i]);
        }
    }
    return active;
}

void MainComponent::pathButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Select Path...", juce::File(), "*");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.isDirectory()) {
                extractor.setCsvPath(result.getFullPathName());
                csvPathButton.setButtonText(extractor.getCsvPath());
            }
        });
}

void MainComponent::releaseResources() {
    extractor.releaseResources();
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    area.reduce(15, 15);

    auto headerArea = area.removeFromTop(45);
    auto buttonsArea = headerArea.removeFromLeft(100);

    settingsBtn.setBounds(buttonsArea.removeFromLeft(40).withSizeKeepingCentre(40, 40));
    buttonsArea.removeFromLeft(10);
    monitorBtn.setBounds(buttonsArea.removeFromLeft(40).withSizeKeepingCentre(40, 40));

    area.removeFromTop(10);

    auto leftArea = area.removeFromLeft(area.getWidth() * 0.33f);
    inputPanel.setBounds(leftArea);

    area.removeFromLeft(10);

    auto bottomArea = area.removeFromBottom(area.getHeight() * 0.40f);
    outputPanel.setBounds(bottomArea);

    area.removeFromBottom(10);

    auto analysisArea = area.removeFromLeft(area.getWidth() * 0.5f);
    analysisPanel.setBounds(analysisArea);

    area.removeFromLeft(10);
    meteringPanel.setBounds(area);

    int tabOffset = 35;
    int padding = 15;

    auto inputContent = inputPanel.getBounds().reduced(padding);
    inputContent.removeFromTop(tabOffset);
    extractor.getAudioPlayer().setBounds(inputContent);

    auto analysisContent = analysisPanel.getBounds().reduced(padding);
    analysisContent.removeFromTop(tabOffset);
    featList.setBounds(analysisContent.removeFromTop(analysisContent.getHeight() * 0.5f - 10));
    analysisContent.removeFromTop(20);
    funcList.setBounds(analysisContent);

    auto meteringContent = meteringPanel.getBounds().reduced(padding);
    meteringContent.removeFromTop(tabOffset);
    waveViewer.setBounds(meteringContent);

    auto outputContent = outputPanel.getBounds().reduced(padding);
    outputContent.removeFromTop(tabOffset);

    auto csvWidth = outputContent.getWidth() * 0.28f;

    auto megaTitlesArea = outputContent.removeFromTop(25);
    batchOutputLabel.setBounds(megaTitlesArea.removeFromLeft(csvWidth));
    megaTitlesArea.removeFromLeft(30);
    liveOutputLabel.setBounds(megaTitlesArea);

    outputContent.removeFromTop(5);

    auto csvCol = outputContent.removeFromLeft(csvWidth).reduced(5, 0);
    auto separatorArea = outputContent.removeFromLeft(30);
    auto liveArea = outputContent;
    auto liveColWidth = liveArea.getWidth() / 3.0f;

    auto oscCol = liveArea.removeFromLeft(liveColWidth).reduced(5, 0);
    auto midiCol = liveArea.removeFromLeft(liveColWidth).reduced(5, 0);
    auto rateCol = liveArea.reduced(5, 0);

    csvLabel.setBounds(csvCol.removeFromTop(25));

    auto oscHead = oscCol.removeFromTop(25);
    oscTitleLabel.setBounds(oscHead.removeFromLeft(45));
    oscCheck.setBounds(oscHead);

    auto midiHead = midiCol.removeFromTop(25);
    midiTitleLabel.setBounds(midiHead.removeFromLeft(45));
    midiCheck.setBounds(midiHead);

    rateLabel.setBounds(rateCol.removeFromTop(25));

    csvSelectFolderLabel.setBounds(csvCol.removeFromTop(20));
    oscIPLabel.setBounds(oscCol.removeFromTop(20));
    midiOutputListLabel.setBounds(midiCol.removeFromTop(20));
    rateCol.removeFromTop(20);

    csvPathButton.setBounds(csvCol.removeFromTop(26));
    oscIPEditor.setBounds(oscCol.removeFromTop(26));
    midiOutputList.setBounds(midiCol.removeFromTop(26));

    csvCol.removeFromTop(10);
    oscCol.removeFromTop(10);
    midiCol.removeFromTop(10);

    csvNameLabel.setBounds(csvCol.removeFromTop(20));
    oscPortLabel.setBounds(oscCol.removeFromTop(20));

    csvNameEditor.setBounds(csvCol.removeFromTop(26));
    oscPortEditor.setBounds(oscCol.removeFromTop(26));

    // new

    csvCol.removeFromTop(10);

    auto exportArea = csvCol.removeFromTop(26);
    exportLabel.setBounds(exportArea.removeFromLeft(50));
    exportModeList.setBounds(exportArea);

    csvCol.removeFromTop(6);

    auto fmtLabels = csvCol.removeFromTop(18);
    auto fmtBoxes = csvCol.removeFromTop(26);
    const int fmtW = fmtBoxes.getWidth() / 3;
    sepLabel.setBounds(fmtLabels.removeFromLeft(fmtW));
    decSepLabel.setBounds(fmtLabels.removeFromLeft(fmtW));
    precisionLabel.setBounds(fmtLabels);
    sepEditor.setBounds(fmtBoxes.removeFromLeft(fmtW).reduced(1, 0));
    decSepEditor.setBounds(fmtBoxes.removeFromLeft(fmtW).reduced(1, 0));
    precisionList.setBounds(fmtBoxes.reduced(1, 0));



    rateCol.removeFromTop(0);
    rateSlider.setBounds(rateCol.removeFromTop(70).withSizeKeepingCentre(70, 70));
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white.withAlpha(0.2f));

    g.drawRoundedRectangle(waveViewer.getBounds().toFloat(), 5.0f, 1.0f);

    auto outBounds = outputPanel.getBounds().reduced(15);
    outBounds.removeFromTop(35);

    float lineX = outBounds.getX() + (outBounds.getWidth() * 0.28f) + 15.0f;

    float lineY = outBounds.getY() + 30.0f;
    float lineBottom = outBounds.getBottom() - 5.0f;

    juce::Colour lineColor = juce::Colours::white.withAlpha(0.15f);
    juce::Colour transparent = juce::Colours::white.withAlpha(0.0f);

    juce::ColourGradient gradient(transparent, lineX, lineY, transparent, lineX, lineBottom, false);
    gradient.addColour(0.2, lineColor);
    gradient.addColour(0.8, lineColor);

    g.setGradientFill(gradient);
    g.drawLine(lineX, lineY, lineX, lineBottom, 1.5f);
}