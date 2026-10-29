#include "AudioPlayer.h"

AudioPlayer::AudioPlayer(juce::AudioFormatManager& manager)
    : state(Stopped), formatManager(manager)
{
    juce::Colour panelBg = juce::Colour::fromString("#FF2A2A30");
    juce::Colour activeBlue = juce::Colour::fromString("#FF2D9CFF");

    addAndMakeVisible(&folderTabBtn);
    folderTabBtn.setSvg(BinaryData::folder_svg, BinaryData::folder_svgSize);
    folderTabBtn.setRadioGroupId(1);
    folderTabBtn.setClickingTogglesState(true);
    folderTabBtn.setToggleState(true, juce::dontSendNotification);
    folderTabBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
    folderTabBtn.setColour(juce::TextButton::buttonColourId, panelBg.brighter(0.1f));
    folderTabBtn.setColour(juce::TextButton::buttonOnColourId, activeBlue);

    folderTabBtn.onClick = [this] {

        if (folderTabBtn.getToggleState() == true) {

            bool wasComingFromLive = !fileListBox.isEnabled();

            setGuiLiveMode(false);
            if (onLiveModeToggled) onLiveModeToggled(false);

            if (!wasComingFromLive || currentFileList.empty()) {
                openButtonClicked();
            }
        }
        };

    addAndMakeVisible(&liveTabBtn);
    liveTabBtn.setSvg(BinaryData::mic_svg, BinaryData::mic_svgSize);
    liveTabBtn.setRadioGroupId(1);
    liveTabBtn.setClickingTogglesState(true);
    liveTabBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
    liveTabBtn.setColour(juce::TextButton::buttonColourId, panelBg.brighter(0.1f));
    liveTabBtn.setColour(juce::TextButton::buttonOnColourId, activeBlue);

    liveTabBtn.onClick = [this] {
        if (liveTabBtn.getToggleState() == true) {
            setGuiLiveMode(true);
            if (onLiveModeToggled) onLiveModeToggled(true);
        }
        };

    addAndMakeVisible(&pathLabel);
    pathLabel.setText("Folder:", juce::dontSendNotification);
    pathLabel.setJustificationType(juce::Justification::centredLeft);
    pathLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    pathLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(&fileListBox);
    fileListBox.setModel(this);
    fileListBox.setRowHeight(25);
    fileListBox.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromString("#FF1E2836")); 

    addAndMakeVisible(&playButton);
    playButton.setSvg(BinaryData::play_svg, BinaryData::play_svgSize);
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setConnectedEdges(juce::Button::ConnectedOnRight);
    playButton.setColour(juce::TextButton::buttonColourId, activeBlue);
    playButton.setEnabled(false);

    addAndMakeVisible(&stopButton);
    stopButton.setSvg(BinaryData::pause_svg, BinaryData::pause_svgSize);
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    stopButton.setColour(juce::TextButton::buttonColourId, activeBlue);
    stopButton.setEnabled(false);

    addAndMakeVisible(&processButton);
    processButton.setButtonText("Process folder");
    processButton.onClick = [this] { processButtonClicked(); };
    processButton.setColour(juce::TextButton::buttonColourId, activeBlue);
    processButton.setEnabled(false);

    transportSource.addChangeListener(this);
}

AudioPlayer::~AudioPlayer() {
    transportSource.setSource(nullptr);
}

void AudioPlayer::setGuiLiveMode(bool isLive) {
    bool isFolder = !isLive;
    fileListBox.setEnabled(isFolder);
    processButton.setEnabled(isFolder && !currentFileList.empty());
    playButton.setEnabled(isFolder && readerSource != nullptr);
    stopButton.setEnabled(isFolder);
    pathLabel.setAlpha(isFolder ? 1.0f : 0.3f);
    fileListBox.setAlpha(isFolder ? 1.0f : 0.5f);
}

void AudioPlayer::resized() {
    auto area = getLocalBounds();

    auto tabArea = area.removeFromTop(30);
    folderTabBtn.setBounds(tabArea.removeFromLeft(tabArea.getWidth() / 2));
    liveTabBtn.setBounds(tabArea);

    area.removeFromTop(10);

    pathLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);

    processButton.setBounds(area.removeFromBottom(35));

    area.removeFromBottom(15);

    auto playArea = area.removeFromBottom(30);
    int halfWidth = playArea.getWidth() / 2;
    playButton.setBounds(playArea.removeFromLeft(halfWidth));
    stopButton.setBounds(playArea); 

    area.removeFromBottom(10);

    fileListBox.setBounds(area);
}

void AudioPlayer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::transparentBlack);
}



void AudioPlayer::prepareToPlay(int samplesPerBlockExpected, double sr) {
    blockSize = samplesPerBlockExpected;
    sampleRate = sr;
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    if (readerSource.get() == nullptr) {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    transportSource.getNextAudioBlock(bufferToFill);
}

void AudioPlayer::releaseResources() {
    transportSource.releaseResources();
}

void AudioPlayer::openButtonClicked() {
    chooser = std::make_unique<juce::FileChooser>("Select folder...", juce::File(), "*");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.isDirectory()) {
                pathLabel.setText("Folder: " + result.getFullPathName(), juce::dontSendNotification);
                currentFileList.clear();
                juce::RangedDirectoryIterator it(result, false, "*.wav;*.mp3;*.flac;*.aif;*.ogg");
                for (const auto& entry : it) {
                    currentFileList.push_back(entry.getFile());
                }
                fileListBox.updateContent();
                fileListBox.repaint();
                processButton.setEnabled(!currentFileList.empty());
            }
        });
}

void AudioPlayer::loadFile(const juce::File& file) {
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr) {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        playButton.setEnabled(true);
        readerSource.reset(newSource.release());
    }
}

void AudioPlayer::processButtonClicked() {
    if (onProcessRequested != nullptr && !currentFileList.empty()) { onProcessRequested(currentFileList); }
}

void AudioPlayer::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &transportSource) {
        if (transportSource.isPlaying()) changeState(Playing);
        else changeState(Stopped);
    }
}

bool AudioPlayer::isPlaying() const { return state == Starting || state == Playing; }

void AudioPlayer::changeState(TransportState newState) {
    if (state != newState) {
        if (newState == Starting && onPlaybackStarted != nullptr && readerSource != nullptr)
            onPlaybackStarted(sampleRate, blockSize);     

        state = newState;
        switch (state) {
        case Stopped:
            stopButton.setEnabled(false);
            playButton.setEnabled(true);
            transportSource.setPosition(0.0);
            if (onPlaybackStopped != nullptr) { onPlaybackStopped(); }
            break;
        case Starting:
            playButton.setEnabled(false);
            transportSource.start();
            break;
        case Playing:
            stopButton.setEnabled(true);
            break;
        case Stopping:
            transportSource.stop();
            break;
        }
    }
}

void AudioPlayer::setInteractionEnabled(bool shouldBeEnabled) {
    if (folderTabBtn.getToggleState()) {
        fileListBox.setEnabled(shouldBeEnabled || isPlaying());
        processButton.setEnabled(shouldBeEnabled && !currentFileList.empty());
        playButton.setEnabled(shouldBeEnabled && readerSource != nullptr);
    }
}

void AudioPlayer::playButtonClicked() { changeState(Starting); }
void AudioPlayer::stopButtonClicked() { changeState(Stopping); }
void AudioPlayer::setProcessEnabled(bool shouldBeEnabled) { processButton.setEnabled(shouldBeEnabled); }
int AudioPlayer::getNumRows() { return (int)currentFileList.size(); }

void AudioPlayer::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowIsSelected) g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    if (rowNumber < (int)currentFileList.size())
        g.drawText(currentFileList[rowNumber].getFileName(), 5, 0, width, height, juce::Justification::centredLeft);
}

void AudioPlayer::selectedRowsChanged(int lastRowSelected) {
    if (lastRowSelected >= 0 && lastRowSelected < (int)currentFileList.size())
        loadFile(currentFileList[lastRowSelected]);
}