#include "MainComponent.h"
#include "ImportFolderPicker.h"
#include "UiTheme.h"
#include <cmath>
#include <memory>
#include "analysis/EssentiaAnalyzer.h"
#include "dsp/graph/AdaptiveMasteringProcessor.h"
#include "export/WavExporter.h"
#include "export/RedBookExporter.h"
#include "project/SideRenderer.h"
#include "dsp/AudioConstants.h"
#include "io/DropPayload.h"
#include "util/AppLog.h"
#include "../locale/CdBurnerLocale.h"
#include "../burn/CdBurnService.h"

namespace cassette
{

MainComponent::MainComponent()
{
    using namespace ui;

    for (auto* c : { static_cast<juce::Component*>(&discSetupPanel),
                     static_cast<juce::Component*>(&dropHero),
                     static_cast<juce::Component*>(&wizardSteps),
                     static_cast<juce::Component*>(&readySummary),
                     static_cast<juce::Component*>(&compareWaveform),
                     static_cast<juce::Component*>(&trackListEditor),
                     static_cast<juce::Component*>(&newButton),
                     static_cast<juce::Component*>(&startButton),
                     static_cast<juce::Component*>(&exportButton),
                     static_cast<juce::Component*>(&burnButton),
                     static_cast<juce::Component*>(&status) })
        addAndMakeVisible(c);

    addChildComponent(mixtapePanel);

    discSetupPanel.setMainScreenMode(true);
    discSetupPanel.setCompactToolbarMode(true);
    discSetupPanel.setMixtapeMode(false);
    discSetupPanel.onSetupChanged = [this] {
        const auto tape = discSetup().getTapeLengthSpec();
        if (mixtapeModeActive && folderScan.has_value() && folderScan->success)
        {
            const bool editorActive = !mixtapeEditor.sideA().empty() || !mixtapeEditor.sideB().empty();
            if (editorActive)
            {
                mixtapeEditor.syncCassettePlan(tape);
                if (mixtapeEditor.hasSideOverflow(tape) || !mixtapeEditor.canPrepare(tape))
                    mixtapeEditor.rebalance(tape);
            }
            trackListEditor.setTapeSpec(tape);
            trackListEditor.refresh();
        }
        refreshFolderFitLabel();
        if (mixtapeModeActive && hasProcessed)
            invalidatePreparedOutput();
        syncLayout();
    };
    Theme::applyLabel(readySummary, Theme::metricFont(), Theme::okGreen());
    readySummary.setVisible(false);

    Theme::styleRecButton(startButton);
    Theme::styleExportButton(exportButton);
    Theme::styleAccentButton(burnButton);
    Theme::styleNeutralButton(newButton);
    newButton.addListener(this);
    startButton.addListener(this);
    exportButton.addListener(this);
    burnButton.addListener(this);
    startButton.setEnabled(false);
    newButton.setEnabled(false);
    exportButton.setEnabled(false);
    burnButton.setEnabled(false);

    Theme::applyLabel(status, Theme::bodyFont(), Theme::textSecondary());

    dropHero.onChooseFolder = [this] { pickImportFolder(); };
    compareWaveform.setShowEmptyDropZone(false);

    mixtapePanel.onFolderSelected = [this](const juce::File& folder) { scanMixFolder(folder); };

    trackListEditor.setVisible(false);
    trackListEditor.setMediaUnitLabel("Disc");
    trackListEditor.setSingleListMode(true);
    previewDeviceManager.initialiseWithDefaultDevices(0, 2);
    trackListEditor.attachToAudioDevice(previewDeviceManager);
    trackListEditor.onLayoutChanged = [this] {
        if (mixtapeModeActive && hasProcessed)
            invalidatePreparedOutput();
        if (folderScan.has_value() && folderScan->success)
            mixtapeEditor.syncCassettePlan(discSetup().getTapeLengthSpec());
        refreshFolderFitLabel();
        syncLayout();
    };

    setStatus({}, Theme::textSecondary());
    refreshUiText();
    startTimerHz(12);
    setWantsKeyboardFocus(true);
    setSize(1180, 820);
    syncLayout();
}

MainComponent::~MainComponent()
{
    stopTimer();
    trackListEditor.shutdownPreviewAudio();
    previewDeviceManager.closeAudioDevice();
}

DiscSetupPanel& MainComponent::discSetup() { return discSetupPanel; }
const DiscSetupPanel& MainComponent::discSetup() const { return discSetupPanel; }

void MainComponent::setStatus(const juce::String& text, juce::Colour colour)
{
    status.setText(text, juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, colour);
    status.setVisible(text.isNotEmpty());
}

void MainComponent::refreshUiText()
{
    newButton.setButtonText(cdb::tr("btn.new"));
    startButton.setButtonText(cdb::tr("btn.prepare"));
    exportButton.setButtonText(cdb::tr("btn.export_wav"));
    burnButton.setButtonText(cdb::tr("btn.burn_cd"));
    discSetupPanel.refreshLocalisedText();
    dropHero.refreshLocalisedText();
    trackListEditor.refreshLocalisedText();
    wizardSteps.repaint();
    syncTransportButtonStyles();
}

void MainComponent::syncTransportButtonStyles()
{
    using ui::Theme;

    const bool newLooksActive = newButton.isEnabled() && (hasSource || hasProcessed);
    Theme::applyTransportButtonStyle(newButton,
                                     newLooksActive ? Theme::TransportButtonStyle::Black
                                                    : Theme::TransportButtonStyle::Neutral,
                                     newButton.isEnabled());
    Theme::applyTransportButtonStyle(startButton, Theme::TransportButtonStyle::Rec, startButton.isEnabled());
    Theme::applyTransportButtonStyle(exportButton, Theme::TransportButtonStyle::Export, exportButton.isEnabled());
    Theme::applyTransportButtonStyle(burnButton, Theme::TransportButtonStyle::Rec, burnButton.isEnabled());

    newButton.repaint();
    startButton.repaint();
    exportButton.repaint();
    burnButton.repaint();
}

void MainComponent::updateWizardState()
{
    wizardSteps.setStepDone(WizardPhase::AddMusic, hasSource);
    wizardSteps.setStepDone(WizardPhase::EditTracks, hasSource);
    wizardSteps.setStepDone(WizardPhase::Preparing, hasProcessed);
    wizardSteps.setStepDone(WizardPhase::ReadyToExport, exportButton.isEnabled());

    const bool busy = isProcessing.load();
    const bool folderBusy = mixtapePanelBusy;
    const bool showTrackEditor = mixtapeModeActive && (hasSource || folderBusy) && !hasProcessed && !busy;

    if (!hasSource && !folderBusy)
        wizardSteps.setPhase(WizardPhase::AddMusic);
    else if (showTrackEditor || (folderBusy && mixtapeModeActive && !hasProcessed))
        wizardSteps.setPhase(WizardPhase::EditTracks);
    else if (!hasProcessed)
        wizardSteps.setPhase(WizardPhase::Preparing);
    else
        wizardSteps.setPhase(WizardPhase::ReadyToExport);

    dropHero.setVisible(!hasSource && !folderBusy);
    trackListEditor.setVisible(showTrackEditor);
    trackListEditor.setLoading(folderBusy && !hasSource);
    compareWaveform.setVisible(hasSource && !showTrackEditor);
    discSetupPanel.setMixtapeMode(mixtapeModeActive);
    discSetupPanel.setCompactToolbarMode(!mixtapeModeActive);

    dropHero.setInteractionEnabled(!busy);
    discSetupPanel.setInteractionEnabled(!busy);
    mixtapePanel.setBusy(busy);

    startButton.setVisible(!hasProcessed || busy);
    startButton.setEnabled(hasSource && !busy && !hasProcessed
                           && (!mixtapeModeActive || folderFitOk));
    trackListEditor.setInterceptsMouseClicks(showTrackEditor, showTrackEditor);
    startButton.setButtonText(cdb::tr("btn.prepare"));
    newButton.setEnabled((hasSource || hasProcessed) && !busy);
    exportButton.setEnabled(!busy && hasProcessed && loadedAudio.has_value());
    burnButton.setEnabled(!busy && hasProcessed && !preparedDiscTracks.empty());

    syncTransportButtonStyles();
}

void MainComponent::updateReadySummary()
{
    readySummary.setVisible(false);
}

void MainComponent::updateWaveformInfo(const AudioFeatures& source, const AudioFeatures* processed)
{
    const auto dur = sourceAudio.has_value()
                         ? sourceAudio->buffer.getNumSamples() / sourceAudio->sampleRate
                         : (loadedAudio.has_value() ? loadedAudio->buffer.getNumSamples() / loadedAudio->sampleRate
                                                    : 0.0);

    WaveformCardInfo before;
    before.title = "Before";
    before.hasAudio = sourceAudio.has_value() || hasSource;
    before.subtitle = juce::String(source.integratedLUFS, 1) + " LUFS  |  "
                      + juce::String(dur / 60.0, 1) + " min";
    compareWaveform.setBeforeInfo(before);

    WaveformCardInfo after;
    after.title = "After";
    if (processed != nullptr)
    {
        after.hasAudio = true;
        after.subtitle = juce::String(processed->integratedLUFS, 1) + " LUFS  |  "
                         + juce::String(processed->truePeakDbfs, 1) + " dBTP";
    }
    compareWaveform.setAfterInfo(after);
}

MasteringOptions MainComponent::currentMasteringOptions() const
{
    return discSetup().getMasteringOptions();
}

void MainComponent::paintProgressBar(juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.isEmpty() || !isProcessing.load())
        return;

    auto r = area;
    ui::Theme::drawPanel(g, r, true);

    const float fill = static_cast<float>(juce::jlimit(0.0, 1.0, progress));
    if (fill > 0.001f)
    {
        auto fillR = r.withWidth(static_cast<int>(r.getWidth() * fill));
        g.setColour(ui::Theme::accent());
        g.fillRect(fillR);
    }

    const int pct = static_cast<int>(std::lround(fill * 100.0));
    g.setColour(fill > 0.55f ? juce::Colours::white : ui::Theme::textPrimary());
    g.setFont(ui::Theme::metricFont());
    g.drawText(juce::String(pct) + "%", r, juce::Justification::centred);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::background());

    g.setColour(ui::Theme::chrome());
    g.fillRect(leftSidebarBounds);

    g.setColour(ui::Theme::border());
    g.drawVerticalLine(leftSidebarBounds.getRight(), 0.0f, static_cast<float>(getHeight()));
    if (kRightSidebarW > 0)
        g.drawVerticalLine(rightSidebarBounds.getX(), 0.0f, static_cast<float>(getHeight()));

    paintProgressBar(g, progressBounds);

    if (activeDropKind != DropPayloadKind::None)
    {
        g.setColour(ui::Theme::accent().withAlpha(0.08f));
        g.fillRect(getLocalBounds().reduced(1));
        g.setColour(ui::Theme::accent());
        g.drawRect(getLocalBounds().reduced(1), 2);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    leftSidebarBounds = bounds.removeFromLeft(kLeftSidebarW);
    if (kRightSidebarW > 0)
        rightSidebarBounds = bounds.removeFromRight(kRightSidebarW);
    else
        rightSidebarBounds = {};
    auto centre = bounds;

    auto sidebar = leftSidebarBounds.reduced(16, 18);

    auto topBar = centre.removeFromTop(56).reduced(14, 12);
    // Keep New aligned with Prepare / Export in the top bar.
    newButton.setBounds(sidebar.getX(), topBar.getY(), sidebar.getWidth(), topBar.getHeight());

    discSetupPanel.setCompactToolbarMode(!mixtapeModeActive);

    startButton.setBounds(topBar.removeFromLeft(112));
    topBar.removeFromLeft(10);
    exportButton.setBounds(topBar.removeFromRight(124));
    topBar.removeFromRight(10);
    burnButton.setBounds(topBar.removeFromRight(112));

    if (kRightSidebarW > 0)
    {
        auto right = rightSidebarBounds.reduced(0, 10);
        discSetupPanel.setBounds(right);
    }

    centre.removeFromTop(2);
    wizardSteps.setBounds(centre.removeFromTop(52).reduced(10, 0));
    centre.removeFromTop(6);

    if (mixtapeModeActive)
    {
        discSetupPanel.setCompactToolbarMode(false);
        const int tapePanelH = discSetupPanel.getPreferredHeight();
        discSetupPanel.setBounds(centre.removeFromTop(tapePanelH).reduced(12, 0));
        centre.removeFromTop(4);
    }

    if (!hasSource)
    {
        auto statusRow = centre.removeFromBottom(28).reduced(12, 4);
        progressBounds = statusRow.removeFromRight(168);
        statusRow.removeFromRight(8);
        status.setBounds(statusRow);
        dropHero.setBounds(centre.reduced(12, 8));
        return;
    }

    if (readySummary.isVisible())
    {
        readySummary.setBounds(centre.removeFromTop(22).reduced(14, 0));
        centre.removeFromTop(6);
    }

    auto statusRow = centre.removeFromBottom(28).reduced(12, 4);
    progressBounds = statusRow.removeFromRight(168);
    statusRow.removeFromRight(8);
    status.setBounds(statusRow);

    if (trackListEditor.isVisible())
    {
        trackListEditor.setBounds(centre.reduced(8, 4));
    }
    else
    {
        compareWaveform.setBounds(centre.reduced(8, 4));
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'N')
    {
        if (newButton.isEnabled())
            newButton.triggerClick();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'O')
    {
        pickImportFolder();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == juce::KeyPress::returnKey)
    {
        if (startButton.isEnabled())
            startButton.triggerClick();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'B')
    {
        if (burnButton.isEnabled())
            burnButton.triggerClick();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'E')
    {
        if (exportButton.isEnabled())
            exportButton.triggerClick();
        return true;
    }
    if (trackListEditor.isVisible() && trackListEditor.keyPressed(key))
        return true;

    if (auto* window = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent()))
    {
#if JUCE_MAC
        if (mods.isCommandDown() && mods.isCtrlDown() && key.getTextCharacter() == 'f')
        {
            window->setFullScreen(!window->isFullScreen());
            return true;
        }
#endif
        if (key.getKeyCode() == juce::KeyPress::F11Key)
        {
            window->setFullScreen(!window->isFullScreen());
            return true;
        }
    }

    return false;
}

void MainComponent::timerCallback()
{
    if (isProcessing.load())
        repaint(progressBounds);
}

void MainComponent::setProgress(double value)
{
    progress = juce::jlimit(0.0, 1.0, value);
    repaint(progressBounds);
}

void MainComponent::syncLayout()
{
    updateWizardState();
    resized();
}

void MainComponent::setUiProcessing(bool processing)
{
    isProcessing.store(processing);
    if (processing)
        progress = 0.0;
    if (!processing)
        refreshFolderFitLabel();
    syncLayout();
    repaint(progressBounds);
}

void MainComponent::resetSession()
{
    if (isProcessing.load())
        return;

    hasSource = false;
    hasProcessed = false;
    mixtapeModeActive = false;
    hasSideB = false;
    mixtapeCassetteCount = 1;
    folderScan.reset();
    mixtapeEditor.clear();
    loadedAudio.reset();
    sourceAudio.reset();
    mixtapeReferenceAudio.reset();
    sideAAudio.reset();
    sideBAudio.reset();
    lastQuality.reset();
    lastProcessedFeatures.reset();
    processingChainOptions = MasteringOptions {};
    loadedFile = juce::File();
    sideAPath = juce::File();
    sideBPath = juce::File();
    sideADurationSec = 0.0;
    sideBDurationSec = 0.0;

    preparedDiscTracks.clear();
    preparedActiveDiscIndex = 0;
    preparedTapeLabel = {};
    compareWaveform.clearAll();
    readySummary.setVisible(false);
    exportButton.setEnabled(false);
    burnButton.setEnabled(false);

    discSetupPanel.setMixtapeMode(false);
    discSetupPanel.setTapeFitSummary({}, true);
    folderFitOk = true;
    mixtapePanel.setFitReport("", true);
    mixtapePanelBusy = false;
    mixtapePanel.setBusy(false);

    setStatus({}, ui::Theme::textSecondary());
    syncLayout();
}

void MainComponent::invalidatePreparedOutput()
{
    hasProcessed = false;
    preparedDiscTracks.clear();
    preparedActiveDiscIndex = 0;
    preparedTapeLabel = {};
    lastQuality.reset();
    lastProcessedFeatures.reset();

    if (sourceAudio.has_value())
    {
        loadedAudio = *sourceAudio;
        compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
    }
    compareWaveform.clearAfter();
    readySummary.setVisible(false);

    setStatus(cdb::tr("status.choose_then_prepare"), ui::Theme::textSecondary());
    syncLayout();
}

void MainComponent::pickImportFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import folder",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        importFolderChooserWildcard());

    chooser->launchAsync(importFolderChooserFlags(),
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto folder = folderFromImportPickerResult(fc.getResult());
                             if (folder.isDirectory())
                                 scanMixFolder(folder);
                         });
}

void MainComponent::refreshFolderFitLabel()
{
    if (!folderScan.has_value() || !folderScan->success)
    {
        folderFitOk = true;
        discSetupPanel.setTapeFitSummary({}, true);
        return;
    }

    const auto tape = discSetup().getTapeLengthSpec();
    const bool editorActive = !mixtapeEditor.sideA().empty() || !mixtapeEditor.sideB().empty();
    const auto report = editorActive ? mixtapeEditor.computeFullFit(tape)
                                     : FolderMixBuilder::analyzeFit(*folderScan, tape);

    folderFitOk = editorActive ? mixtapeEditor.canPrepare(tape) : report.fits;
    mixtapePanel.setFitReport(report.summary(), folderFitOk);
    mixtapePanel.setBuildEnabled(folderFitOk && !isProcessing.load());
    trackListEditor.setTapeSpec(tape);
}

void MainComponent::scanMixFolder(const juce::File& folder)
{
    log("UI: Import folder requested - " + folder.getFullPathName());

    folderScan.reset();
    hasProcessed = false;
    hasSource = false;
    mixtapePanelBusy = true;
    mixtapePanel.setBusy(true);
    mixtapeModeActive = true;
    discSetupPanel.setMixtapeMode(true);
    setStatus(cdb::trf("status.scanning", folder.getFileName()), ui::Theme::accent());
    syncLayout();

    worker.enqueue([this, folder]() {
        const auto scan = FolderMixBuilder::scanFolder(folder);
        juce::MessageManager::callAsync([this, scan, folder]() {
            mixtapePanelBusy = false;
            mixtapePanel.setBusy(false);
            if (!scan.success)
            {
                log("UI: folder scan failed - " + scan.error);
                mixtapePanel.setFitReport(scan.error, false);
                setStatus(scan.error, ui::Theme::failRed());
                syncLayout();
                return;
            }
            folderScan = scan;
            mixtapePanel.setFolderScan(scan, folder);
            hasSource = true;
            log("UI: folder scan OK - " + juce::String(scan.tracks.size()) + " tracks");

            const auto fitReport = FolderMixBuilder::analyzeFit(scan, discSetup().getTapeLengthSpec());
            mixtapeEditor.loadFromScan(scan, fitReport);
            trackListEditor.setController(&mixtapeEditor);
            trackListEditor.setLoading(false);
            refreshFolderFitLabel();
            trackListEditor.refresh();
            setStatus({}, ui::Theme::textSecondary());
            syncLayout();
        });
    });
}

void MainComponent::loadAudioFile(const juce::File& file)
{
    loadedFile = file;
    loadedAudio.reset();
    sourceAudio.reset();
    hasProcessed = false;
    hasSource = false;
    preparedDiscTracks.clear();
    preparedActiveDiscIndex = 0;
    lastQuality.reset();
    lastProcessedFeatures.reset();
    hasSideB = false;
    mixtapeModeActive = false;
    mixtapeReferenceAudio.reset();
    discSetupPanel.setMixtapeMode(false);
    compareWaveform.clearAfter();
    exportButton.setEnabled(false);
    setStatus(cdb::trf("status.loading", file.getFileName()), ui::Theme::accent());

    worker.enqueue([this, file]() {
        const auto result = AudioFileLoader::loadToBufferWithDiagnostics(file);
        juce::MessageManager::callAsync([this, file, result]() {
            if (!result.audio.hasValue())
            {
                setStatus(cdb::trf("status.load_failed", result.error), ui::Theme::failRed());
                return;
            }

            loadedAudio = *result.audio;
            sourceAudio = *result.audio;
            hasSource = true;
            compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
            compareWaveform.clearAfter();

            const auto features = EssentiaAnalyzer::extractFeatures(loadedAudio->buffer, loadedAudio->sampleRate);
            updateWaveformInfo(features, nullptr);

            setStatus(cdb::trf("status.added", file.getFileName()), ui::Theme::okGreen());
            syncLayout();
        });
    });
}

void MainComponent::startProcessing()
{
    if (!loadedAudio.has_value())
    {
        setStatus(cdb::tr("status.add_music_first"), ui::Theme::warnAmber());
        return;
    }
    if (isProcessing.load())
        return;

    const auto profile = discSetup().getCassetteProfile();
    const auto options = currentMasteringOptions();
    const auto fileName = loadedFile.getFileName();

    if (!sourceAudio.has_value())
        sourceAudio = *loadedAudio;

    auto audioCopy = *loadedAudio;
    setUiProcessing(true);
    setStatus(cdb::tr("status.preparing"), ui::Theme::accent());
    wizardSteps.setPhase(WizardPhase::Preparing);

    worker.enqueue([this, audioCopy, profile, options, fileName]() mutable {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        juce::MessageManager::callAsync([this]() { setProgress(0.05); });

        juce::AudioBuffer<float> reference;
        reference.makeCopyOf(audioCopy.buffer);
        const auto features = EssentiaAnalyzer::extractFeatures(reference, audioCopy.sampleRate);
        juce::MessageManager::callAsync([this]() { setProgress(0.18); });

        const auto mastered = AdaptiveMasteringProcessor::process(
            audioCopy.buffer, profile, options, audioCopy.sampleRate);
        juce::MessageManager::callAsync([this]() { setProgress(0.82); });

        juce::MessageManager::callAsync([this]() { setProgress(0.96); });
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        juce::MessageManager::callAsync([this, audioCopy, mastered, fileName, ms, features, profile]() mutable {
            setProgress(1.0);
            loadedAudio = std::move(audioCopy);
            lastQuality = mastered.quality;
            hasProcessed = true;
            preparedTapeLabel = profile.displayName;

            if (sourceAudio.has_value())
                compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
            compareWaveform.setAfterBuffer(loadedAudio->buffer, loadedAudio->sampleRate);

            lastProcessedFeatures = mastered.featuresAfter;
            updateWaveformInfo(features, &mastered.featuresAfter);

            juce::ignoreUnused(ms);
            finishProcessing(true, {});
            exportButton.setEnabled(true);
            updateReadySummary();
            syncLayout();
        });
    });
}

void MainComponent::finishProcessing(bool success, const juce::String& message)
{
    setUiProcessing(false);
    setStatus(message, success ? ui::Theme::okGreen() : ui::Theme::failRed());
}

void MainComponent::startFolderMixBuild()
{
    if (!folderScan.has_value() || !folderScan->success)
    {
        setStatus(cdb::tr("status.pick_folder"), ui::Theme::warnAmber());
        return;
    }

    const auto tape = discSetup().getTapeLengthSpec();
    if (!mixtapeEditor.canPrepare(tape))
    {
        const auto fit = mixtapeEditor.computeFit(tape);
        setStatus(fit.summary(), ui::Theme::warnAmber());
        return;
    }
    if (isProcessing.load())
        return;

    const auto profile = discSetup().getCassetteProfile();
    auto options = currentMasteringOptions();
    options.skipQualityCompare = true;

    mixtapeEditor.syncCassettePlan(tape);
    const auto scanCopy = mixtapeEditor.mergedFullScan();
    const int cassetteCount = mixtapeEditor.getCassetteCount();
    std::vector<MixtapeEditController::LayoutSnapshot> cassetteLayouts;
    cassetteLayouts.reserve(static_cast<size_t>(cassetteCount));
    for (int i = 0; i < cassetteCount; ++i)
        cassetteLayouts.push_back(mixtapeEditor.layoutForCassette(i));
    const auto projectName = mixtapePanel.currentFolder().getFileName();
    const juce::File outFolder = mixtapePanel.currentFolder();

    setUiProcessing(true);
    wizardSteps.setPhase(WizardPhase::Preparing);
    setStatus(cassetteCount > 1 ? cdb::trf("status.preparing_discs", cassetteCount)
                                : cdb::tr("status.preparing_tracks"),
              ui::Theme::accent());

    log("UI: Build mixtape started - " + juce::String(scanCopy.tracks.size()) + " tracks, "
        + juce::String(cassetteCount) + " disc(s), out=" + outFolder.getFullPathName());

    worker.enqueue([this,
                    cassetteLayouts,
                    profile,
                    options,
                    projectName,
                    outFolder,
                    cassetteCount]() {
        try
        {
            ScopedTimer buildTimer("folder-build", projectName + " (" + juce::String(cassetteCount) + " disc(s))");
            const double sampleRate = kProjectSampleRate;

            std::vector<FolderTrackInfo> allTracks;
            for (const auto& layout : cassetteLayouts)
            {
                for (const auto& t : layout.sideA)
                    allTracks.push_back(t);
                for (const auto& t : layout.sideB)
                    allTracks.push_back(t);
            }
            const int totalTracks = static_cast<int>(allTracks.size());

            const auto trackProgress = [this, totalTracks](int tracksDone, const juce::String& title) {
                const auto msg = "Track " + juce::String(tracksDone) + "/" + juce::String(totalTracks) + ": " + title;
                const double pct = juce::jlimit(0.0, 0.98, static_cast<double>(tracksDone) / static_cast<double>(totalTracks));
                juce::MessageManager::callAsync([this, msg, pct]() {
                    setProgress(pct);
                    status.setText(msg, juce::dontSendNotification);
                    status.setColour(juce::Label::textColourId, ui::Theme::accent());
                });
            };

            juce::String workerError;
            std::shared_ptr<RenderResult> previewResult;
            juce::File previewTrackFile;
            int globalTrackIndex = 0;
            std::vector<juce::Array<juce::File>> discTrackFiles(static_cast<size_t>(cassetteCount));

            for (int cassetteIdx = 0; cassetteIdx < cassetteCount && workerError.isEmpty(); ++cassetteIdx)
            {
                const auto& layout = cassetteLayouts[static_cast<size_t>(cassetteIdx)];
                const juce::String preparedFolderName =
                    FolderMixBuilder::preparedTracksFolderName(projectName, cassetteIdx, cassetteCount);
                const juce::File preparedDir = outFolder.getChildFile(preparedFolderName);
                if (!preparedDir.createDirectory())
                {
                    workerError = "Failed to create " + preparedDir.getFullPathName();
                    break;
                }

                std::vector<FolderTrackInfo> discTracks;
                discTracks.insert(discTracks.end(), layout.sideA.begin(), layout.sideA.end());
                discTracks.insert(discTracks.end(), layout.sideB.begin(), layout.sideB.end());

                int discTrackNum = 0;
                for (const auto& track : discTracks)
                {
                    ++globalTrackIndex;
                    ++discTrackNum;
                    trackProgress(globalTrackIndex, track.displayName);

                    TapeClip clip;
                    clip.sourceFile = track.file;
                    clip.displayTitle = track.displayName;
                    clip.durationSec = track.durationSec;
                    clip.startTimeSec = 0.0;
                    clip.trackIndex = globalTrackIndex;

                    const bool captureReference = previewResult == nullptr;
                    auto rendered = SideRenderer::renderClip(clip,
                                                           profile,
                                                           options,
                                                           0.0f,
                                                           sampleRate,
                                                           captureReference);
                    if (!rendered.success)
                    {
                        workerError = rendered.error;
                        break;
                    }

                    const auto outFile =
                        preparedDir.getChildFile(FolderMixBuilder::preparedTrackFilename(discTrackNum, track.displayName));
                    if (!RedBookExporter::writeRedBookWav(outFile, rendered.buffer, rendered.sampleRate))
                    {
                        workerError = "Failed to write " + outFile.getFileName();
                        break;
                    }

                    discTrackFiles[static_cast<size_t>(cassetteIdx)].add(outFile);

                    if (previewResult == nullptr)
                    {
                        previewResult = std::make_shared<RenderResult>(std::move(rendered));
                        previewTrackFile = outFile;
                    }
                }
            }

            juce::MessageManager::callAsync([this]() { setProgress(0.99); });

            juce::MessageManager::callAsync([this,
                                             previewResult,
                                             previewTrackFile,
                                             workerError,
                                             profile,
                                             cassetteCount,
                                             discTrackFiles]() mutable {
                if (workerError.isNotEmpty())
                {
                    finishProcessing(false, workerError);
                    return;
                }

                if (previewResult == nullptr)
                {
                    finishProcessing(false, "No prepared tracks produced");
                    return;
                }

                mixtapeCassetteCount = cassetteCount;
                preparedDiscTracks = std::move(discTrackFiles);
                preparedActiveDiscIndex = 0;

                LoadedAudio preview;
                preview.buffer = std::move(previewResult->buffer);
                preview.sampleRate = previewResult->sampleRate;
                sideAAudio = std::move(preview);
                sideAPath = previewTrackFile;
                sideADurationSec = sideAAudio->buffer.getNumSamples() / sideAAudio->sampleRate;
                hasSideB = false;
                sideBAudio.reset();
                sideBPath = juce::File();
                sideBDurationSec = 0.0;

                if (previewResult->referenceBuffer.getNumSamples() > 0)
                {
                    LoadedAudio reference;
                    reference.buffer = std::move(previewResult->referenceBuffer);
                    reference.sampleRate = previewResult->sampleRate;
                    mixtapeReferenceAudio = std::move(reference);
                }

                showMixtapeSide(false);
                hasProcessed = true;
                preparedTapeLabel = profile.displayName;
                exportButton.setEnabled(true);
                burnButton.setEnabled(!preparedDiscTracks.empty());

                updateReadySummary();
                const juce::String doneMsg = cassetteCount > 1
                                                 ? juce::String(cassetteCount) + " prepared track folders saved next to your music"
                                                 : cdb::tr("status.prepared_tracks_done");
                setProgress(1.0);
                finishProcessing(true, doneMsg);
            });
        }
        catch (const std::exception& e)
        {
            juce::MessageManager::callAsync([this, msg = juce::String(e.what())]() {
                finishProcessing(false, "Processing failed: " + msg);
            });
        }
        catch (...)
        {
            juce::MessageManager::callAsync([this]() {
                finishProcessing(false, "Processing failed unexpectedly");
            });
        }
    });
}

void MainComponent::showMixtapeSide(bool sideB)
{
    if (!sideAAudio.has_value())
        return;

    const auto& audio = sideB && sideBAudio.has_value() ? *sideBAudio : *sideAAudio;
    loadedAudio = audio;
    loadedFile = sideB ? sideBPath : sideAPath;

    if (!sideB && mixtapeReferenceAudio.has_value())
    {
        sourceAudio = *mixtapeReferenceAudio;
        compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
    }
    else
    {
        juce::AudioBuffer<float> empty;
        compareWaveform.setBeforeBuffer(empty, audio.sampleRate);
    }

    compareWaveform.setAfterBuffer(audio.buffer, audio.sampleRate);

    const double dur = audio.buffer.getNumSamples() / audio.sampleRate;

    if (sourceAudio.has_value() && sourceAudio->buffer.getNumSamples() > 0)
    {
        const auto srcExcerpt = EssentiaAnalyzer::excerpt(sourceAudio->buffer, sourceAudio->sampleRate);
        const auto procExcerpt = EssentiaAnalyzer::excerpt(audio.buffer, audio.sampleRate);
        const auto srcFeatures = EssentiaAnalyzer::extractFeaturesForMastering(srcExcerpt, sourceAudio->sampleRate);
        const auto procFeatures = EssentiaAnalyzer::extractFeaturesForMastering(procExcerpt, audio.sampleRate);
        lastProcessedFeatures = procFeatures;
        updateWaveformInfo(srcFeatures, &procFeatures);

        WaveformCardInfo before;
        before.title = "Before";
        before.hasAudio = true;
        before.subtitle = juce::String(srcFeatures.integratedLUFS, 1) + " LUFS  |  "
                          + juce::String(dur / 60.0, 1) + " min";
        compareWaveform.setBeforeInfo(before);

        WaveformCardInfo after;
        after.title = "After";
        after.hasAudio = true;
        after.subtitle = juce::String(procFeatures.integratedLUFS, 1) + " LUFS  |  "
                         + juce::String(procFeatures.truePeakDbfs, 1) + " dBTP";
        compareWaveform.setAfterInfo(after);
    }
    else
    {
        const auto procExcerpt = EssentiaAnalyzer::excerpt(audio.buffer, audio.sampleRate);
        const auto features = EssentiaAnalyzer::extractFeaturesForMastering(procExcerpt, audio.sampleRate);
        lastProcessedFeatures = features;
        updateWaveformInfo(features, &features);
    }
}

void MainComponent::updateDropHighlight(const juce::StringArray& files, bool active)
{
    activeDropKind = active ? classifyDropPayload(files) : DropPayloadKind::None;
    dropHero.setDragHighlight(active, activeDropKind);
    compareWaveform.setDragHighlight(activeDropKind != DropPayloadKind::None, activeDropKind);
    repaint();
}

bool MainComponent::isInterestedInDrop(const juce::StringArray& files) const
{
    if (isProcessing.load())
        return false;

    return isDropPayloadInterested(files);
}


void MainComponent::startCdBurn()
{
    if (preparedDiscTracks.empty())
    {
        setStatus(cdb::tr("status.prepare_first"), ui::Theme::warnAmber());
        return;
    }

    if (isProcessing.load())
        return;

    const int discIndex = juce::jlimit(0, static_cast<int>(preparedDiscTracks.size()) - 1, preparedActiveDiscIndex);
    const auto& tracks = preparedDiscTracks[static_cast<size_t>(discIndex)];
    if (tracks.isEmpty())
    {
        setStatus(cdb::tr("status.prepare_first"), ui::Theme::warnAmber());
        return;
    }

    auto devices = CdBurnService::listDevices();
    if (devices.empty())
    {
        setStatus(cdb::tr("status.no_burner") + " " + CdBurnService::platformBurnHint(), ui::Theme::warnAmber());
        return;
    }

    CdBurnDevice device = devices.front();
    if (devices.size() > 1)
    {
        juce::PopupMenu menu;
        for (int i = 0; i < static_cast<int>(devices.size()); ++i)
            menu.addItem(i + 1, devices[static_cast<size_t>(i)].displayName);
        menu.showMenuAsync(juce::PopupMenu::Options(), [this, devices, tracks, discIndex](int result) {
            if (result <= 0)
                return;
            const auto device = devices[static_cast<size_t>(result - 1)];
            startCdBurnWithDevice(device, tracks, discIndex);
        });
        return;
    }

    startCdBurnWithDevice(device, tracks, discIndex);
}

void MainComponent::startCdBurnWithDevice(const CdBurnDevice& device,
                                          const juce::Array<juce::File>& tracks,
                                          int discIndex)
{
    setUiProcessing(true);
    setStatus(cdb::tr("status.burning_cd"), ui::Theme::accent());

    worker.enqueue([this, device, tracks, discIndex]() {
        const auto result = CdBurnService::burnAudioDisc(
            device,
            tracks,
            [this](const CdBurnProgress& progress) {
                juce::MessageManager::callAsync([this, progress]() {
                    setProgress(progress.overallPercent);
                    if (progress.message.isNotEmpty())
                        setStatus(progress.message, ui::Theme::accent());
                });
            });

        juce::MessageManager::callAsync([this, result, discIndex]() {
            setProgress(1.0);
            if (result.success)
            {
                if (discIndex + 1 < static_cast<int>(preparedDiscTracks.size()))
                {
                    preparedActiveDiscIndex = discIndex + 1;
                    setStatus(cdb::trf("status.burn_disc_done_next", discIndex + 2), ui::Theme::okGreen());
                }
                else
                {
                    setStatus(cdb::tr("status.burn_complete"), ui::Theme::okGreen());
                }
            }
            else
            {
                setStatus(result.error.isNotEmpty() ? result.error : cdb::tr("status.burn_failed"),
                          ui::Theme::failRed());
            }
            setUiProcessing(false);
            syncLayout();
        });
    });
}

void MainComponent::exportWav()
{
    if (!loadedAudio.has_value())
    {
        setStatus(cdb::tr("status.prepare_first"), ui::Theme::warnAmber());
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export WAV",
        loadedFile.getParentDirectory().exists() ? loadedFile.getParentDirectory()
                                                 : juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto out = fc.getResult();
                             if (out == juce::File())
                                 return;

                             juce::AudioBuffer<float> exportBuffer;
                             exportBuffer.makeCopyOf(loadedAudio->buffer);

                             if (WavExporter::writeWav32Float(out, exportBuffer, loadedAudio->sampleRate))
                             {
                                 wizardSteps.setStepDone(WizardPhase::ReadyToExport, true);
                                 setStatus(cdb::trf("status.exported", out.getFileName()), ui::Theme::okGreen());
                                 updateWizardState();
                             }
                             else
                                 setStatus(cdb::tr("status.export_failed"), ui::Theme::failRed());
                         });
}

bool MainComponent::isInterestedInAudioFileDrag(const juce::StringArray& files)
{
    return isInterestedInDrop(files);
}

void MainComponent::handleAudioFilesDropped(const juce::StringArray& files, int, int)
{
    if (isProcessing.load())
        return;

    for (const auto& f : files)
    {
        if (AudioFileLoader::normaliseDroppedPath(f).isDirectory())
        {
            scanMixFolder(AudioFileLoader::normaliseDroppedPath(f));
            return;
        }
    }

    const auto file = AudioFileLoader::pickFirstAudioFile(files);
    if (file.existsAsFile())
        loadAudioFile(file);
    else
        setStatus(cdb::tr("status.unsupported_drop"), ui::Theme::warnAmber());
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return isInterestedInDrop(files);
}

void MainComponent::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (!isInterestedInDrop(files))
        return;
    ++windowDragDepth;
    updateDropHighlight(files, true);
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    if (windowDragDepth <= 0)
        return;
    --windowDragDepth;
    if (windowDragDepth == 0)
        updateDropHighlight({}, false);
}

void MainComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    windowDragDepth = 0;
    updateDropHighlight({}, false);
    handleAudioFilesDropped(files, x, y);
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &newButton)
    {
        resetSession();
        return;
    }
    if (button == &startButton)
    {
        if (mixtapeModeActive && folderScan.has_value())
            startFolderMixBuild();
        else
            startProcessing();
        return;
    }
    if (button == &burnButton)
    {
        startCdBurn();
        return;
    }
    if (button == &exportButton)
    {
        exportWav();
        return;
    }
}

}
