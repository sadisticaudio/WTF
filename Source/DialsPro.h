#pragma once
#include "../../Deviant/Source/Dials.h"
#include "WavePad.h"
namespace sadistic {
    
    struct Dials2 : Component {
        Dials2(WaveTableManager& m, int idx) : mgmt(m), button(makeLabel(getFxName(currentEffect = idx))) {
            
            button.onClick = [&] { switchEffect(++currentEffect%=numFX); button.label.label.setColour(Label::backgroundColourId, button.colour); };
            
            forEach ([] (auto& knob) {
                knob.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
                knob.setDoubleClickReturnValue(true,1.0f,ModifierKeys::altModifier);
                knob.setMouseDragSensitivity (100);
                knob.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
                knob.setScrollWheelEnabled(true);
                knob.setRotaryParameters(degreesToRadians(225.f), degreesToRadians(495.f), true);
                knob.setTextValueSuffix("Hz");
            }, blendKnob, driveKnob, deviateKnob, lowKnob, highKnob);
            
            driveKnob.setLookAndFeel(&elaf);
            deviateKnob.setLookAndFeel(&elaf);
            lowKnob.setLookAndFeel(&alaf);
            highKnob.setLookAndFeel(&alaf);
            
            blendKnob.setLookAndFeel(&alaf);
            blendKnob.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
            deviateKnob.setTextValueSuffix("%");
            blendKnob.setTextValueSuffix("%");
            driveKnob.setTextValueSuffix("wtPos");
            
            driveKnob.setRotaryParameters(degreesToRadians(0.f), 5.81333f, true);
            deviateKnob.setRotaryParameters(degreesToRadians(180.f), degreesToRadians(510.f), true);
            
            forEach ([] (auto& label) { label.setJustificationType(Justification::centred);
                label.setColour(juce::Label::textColourId, Colours::lightgrey); }, valueLabel, suffixLabel, lLabel, lSuffix, rLabel, rSuffix);
            
            driveKnob.onDragStart = [&,this]() {
                hideValue(valueLabel, suffixLabel);
                suffixLabel.setTransform(AffineTransform::rotation(MathConstants<float>::twoPi * float(driveKnob.getValue()/driveKnob.getMaximum()), suffixLabel.getBounds().toFloat().getCentreX(), suffixLabel.getBounds().toFloat().getCentreY()));
                showInteger111Value(driveKnob, valueLabel, suffixLabel);
                repaint(); };
            driveKnob.onValueChange = [&,this]() {
                suffixLabel.setTransform(AffineTransform::rotation(MathConstants<float>::twoPi * float(driveKnob.getValue()/driveKnob.getMaximum()), suffixLabel.getBounds().toFloat().getCentreX(), suffixLabel.getBounds().toFloat().getCentreY()));
                showInteger111Value(driveKnob, valueLabel, suffixLabel);
                repaint(); };
            driveKnob.onDragEnd = [&,this]() { hideValue(valueLabel, suffixLabel);};
            
            deviateKnob.onDragStart = [&,this]() {
                hideValue(valueLabel, suffixLabel);
                suffixLabel.setTransform(AffineTransform::rotation(MathConstants<float>::twoPi * float(deviateKnob.getValue()/deviateKnob.getMaximum()), suffixLabel.getBounds().toFloat().getCentreX(), suffixLabel.getBounds().toFloat().getCentreY()));
                showIntegerValue(deviateKnob, valueLabel, suffixLabel);
                repaint(); };
            deviateKnob.onValueChange = [&,this]() {
                suffixLabel.setTransform(AffineTransform::rotation(MathConstants<float>::twoPi * float(deviateKnob.getValue()/deviateKnob.getMaximum()), suffixLabel.getBounds().toFloat().getCentreX(), suffixLabel.getBounds().toFloat().getCentreY()));
                showIntegerValue(deviateKnob, valueLabel, suffixLabel);
                repaint(); };
            deviateKnob.onDragEnd = [&,this]() { hideValue(valueLabel, suffixLabel);};
            
            blendKnob.onDragStart = [&,this]() { hideValue(valueLabel, suffixLabel); showIntegerValue(blendKnob, valueLabel, suffixLabel);};
            blendKnob.onValueChange = [&,this]() { showIntegerValue(blendKnob, valueLabel, suffixLabel); };
            blendKnob.onDragEnd = [&,this]() { hideValue(valueLabel, suffixLabel); };
            
            lowKnob.onDragStart = [&,this]() { hideValue(valueLabel, suffixLabel); showHzValue(lowKnob, lLabel, lSuffix);};
            lowKnob.onValueChange = [&,this]() {
                if (highKnob.getValue() < lowKnob.getValue())
                    highKnob.setValue(lowKnob.getValue(), sendNotification);
                showHzValue(lowKnob, lLabel, lSuffix); };
            lowKnob.onDragEnd = [&,this]() { hideValue(lLabel, lSuffix); hideValue(rLabel, rSuffix); };
            
            highKnob.onDragStart = [&,this]() { hideValue(valueLabel, suffixLabel); showHzValue(highKnob, rLabel, rSuffix);};
            highKnob.onValueChange = [&,this]() {
                if (highKnob.getValue() < lowKnob.getValue())
                    lowKnob.setValue(highKnob.getValue(), sendNotification);
                showHzValue(highKnob, rLabel, rSuffix); };
            highKnob.onDragEnd = [&,this]() { hideValue(rLabel, rSuffix); hideValue(lLabel, lSuffix); };
            
            addAllAndMakeVisible(*this, deviateKnob, driveKnob, blendKnob, driveSVG, deviationSVG, suffixLabel, valueLabel, lLabel, rLabel, phaseBox);
            
            addChildComponent(lowKnob);
            addChildComponent(highKnob);
            
            blendAttachment = std::make_unique<APVTS::SliderAttachment>(mgmt.apvts, "mainBlend", blendKnob);
            
            setMouseOverLabels(lowKnob, "Low", "Cutoff");
            setMouseOverLabels(highKnob, "High", "Cutoff");
            setMouseOverLabels(blendKnob, "Main", "Blend");
            
            lowKnob.showMouseOver = [&] { if(mouseOverActive) showMouseOverLabels(lowKnob, valueLabel, suffixLabel); repaint(); };
            lowKnob.hideMouseOver = [&] { if(mouseOverActive) hideMouseOverLabels(lowKnob, valueLabel, suffixLabel); repaint(); };
            highKnob.showMouseOver = [&] { if(mouseOverActive) showMouseOverLabels(highKnob, valueLabel, suffixLabel); repaint(); };
            highKnob.hideMouseOver = [&] { if(mouseOverActive) hideMouseOverLabels(highKnob, valueLabel, suffixLabel); repaint(); };
            driveKnob.showMouseOver = [&] { if(mouseOverActive) showMouseOverLabels(driveKnob, valueLabel, suffixLabel); repaint(); };
            driveKnob.hideMouseOver = [&] { if(mouseOverActive) hideMouseOverLabels(driveKnob, valueLabel, suffixLabel); repaint(); };
            deviateKnob.showMouseOver = [&] { if(mouseOverActive) showMouseOverLabels(deviateKnob, valueLabel, suffixLabel); repaint(); };
            deviateKnob.hideMouseOver = [&] { if(mouseOverActive) hideMouseOverLabels(deviateKnob, valueLabel, suffixLabel); repaint(); };
            blendKnob.showMouseOver = [&] { if(mouseOverActive) showMouseOverLabels(blendKnob, valueLabel, suffixLabel); repaint(); };
            blendKnob.hideMouseOver = [&] { if(mouseOverActive) hideMouseOverLabels(blendKnob, valueLabel, suffixLabel); repaint(); };
            
            popupMenu.toggleMouseOverEnabled = [&]{ mouseOverActive = mouseOverActive ? false : true; repaint(); };
            popupMenu.toggleFilterControls = [&]{ filterControlsActive = filterControlsActive ? false : true;
                forEach ([ctrls = filterControlsActive] (auto& knob) { knob.setVisible(ctrls); }, lowKnob, highKnob);
                repaint(); };
            popupMenu.loadCallback = [&,this] (const FileChooser& chooser) {
                if (!chooser.getResults().isEmpty()) {
                    if (mgmt.loadTable(chooser.getResult()))
                        juce::AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Table Loaded", "!");
                    else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not loaded correctly", "!");
                }
                else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not choosen correctly", "!");
            };
            popupMenu.loadFile = [&,this](){
                popupMenu.fc = std::make_unique<FileChooser>("Load Wave Table (.wav)", File(), "*.wav");
                popupMenu.fc->launchAsync (FileBrowserComponent::openMode |
                                 FileBrowserComponent::canSelectFiles,
                                 popupMenu.loadCallback); };
            
            switchEffect(idx);
        }
        void switchEffect(int idx) {
            mgmt.apvts.state.setProperty(Identifier("currentScreen"), var(int(currentEffect)), nullptr);
            
            button.label.set(makeLabel(getFxName(idx)), Colours::black, Colours::grey.darker());
            forEach ([] (auto& attachment) { attachment.reset(); }, driveAttachment, deviationAttachment, lowAttachment, highAttachment);
            driveAttachment = std::make_unique<APVTS::SliderAttachment>(mgmt.apvts, "dynamicWaveShaperwtPos", driveKnob);
            deviationAttachment = std::make_unique<APVTS::SliderAttachment>(mgmt.apvts, "dynamicWaveShaperBlend", deviateKnob);
            lowAttachment = std::make_unique<APVTS::SliderAttachment>(mgmt.apvts, "dynamicWaveShaperPreCutoff", lowKnob);
            highAttachment = std::make_unique<APVTS::SliderAttachment>(mgmt.apvts, "dynamicWaveShaperPostCutoff", highKnob);
            
            lowKnob.setNormalisableRange({ 800.0, 8000.0, 0.0, 0.3 });
            highKnob.setNormalisableRange({ 1000.0, 20000.0, 0.0, 0.3 });
            driveKnob.setNormalisableRange({ 0.0, 1.0, 0.0 });
            deviateKnob.setNormalisableRange({ 0.0, 1.0, 0.0 });
            
            setMouseOverLabels(driveKnob, getFxID(idx).trimCharactersAtStart("static"), "Wavetable Position");
            setMouseOverLabels(deviateKnob, getFxID(idx).trimCharactersAtStart("static"), "Wave Shaper Blend");
            
            hideValue(valueLabel, suffixLabel);
            hideValue(lLabel, lSuffix);
            hideValue(rLabel, rSuffix);
            driveSVG.setVisible(false);
            deviationSVG.setVisible(false);
        }
        
        void resized() override {
            const auto bounds { getBounds() };
            driveKnob.setMouseDragSensitivity(static_cast<int>((double)bounds.getHeight() * 0.98));
            deviateKnob.setMouseDragSensitivity(static_cast<int>((double)bounds.getHeight() * 0.98));
            
            auto h { getHeight() }, w { getWidth() }, diameter { jmax(5 * w / 8, 5 * h / 4) };
            
            driveKnob.setSize(diameter, diameter);
            deviateKnob.setSize(diameter, diameter);
            driveKnob.setTopRightPosition(getWidth()/2 - 50, h/2 - diameter/2);
            deviateKnob.setTopLeftPosition(w/2 + 50, h/2 - diameter/2);
            Rectangle<int> r { bounds.getX(), bounds.getY(), (driveKnob.getRight() - 40) - bounds.getX(), h };
            r.reduce(0, jmax(r.getHeight()/6, r.getWidth()/6));
            
            auto leftArea { r }, rightArea { r.withX(deviateKnob.getX() + 40) };
            
            valueLabel.setBounds(leftArea);
            suffixLabel.setBounds(rightArea);
            lLabel.setBounds(leftArea);
            rLabel.setBounds(rightArea);
            
            lSuffix.setSize(lLabel.getWidth()/3, lLabel.getHeight()/3);
            lSuffix.setTopLeftPosition(lLabel.getRight(), (lLabel.getHeight()/4) * 3);
            rSuffix.setSize(rLabel.getWidth()/3, rLabel.getHeight()/3);
            rSuffix.setTopLeftPosition(rLabel.getRight(), (rLabel.getHeight()/4) * 3);
            
            driveSVG.setBounds(deviateKnob.getBounds().reduced(driveKnob.getWidth()/6));
            deviationSVG.setBounds(deviateKnob.getBounds().reduced(driveKnob.getWidth()/6));
            auto blendBounds { Rectangle<int>(static_cast<int>((float)(deviateKnob.getX() - driveKnob.getRight() + getWidth()/8) * (float)getHeight()/(float)driveKnob.getHeight()), 20) };
            blendKnob.setSize(blendBounds.getWidth(), blendBounds.getHeight());
            blendKnob.setCentrePosition(bounds.getCentre().x, blendBounds.getHeight()/2);
            
            forEach ([] (Label& label) { label.setFont(getSadisticFont().withHeight(static_cast<float>(jmin(label.getHeight() * 2 / 3, label.getWidth() * 2 / 3)))); label.setColour(Label::ColourIds::textColourId, Colours::grey); }, valueLabel, suffixLabel, lLabel, lSuffix, rLabel, rSuffix);
            
            deviateKnob.setMouseDragSensitivity(bounds.getHeight() / 2);
            driveKnob.setMouseDragSensitivity(bounds.getHeight() / 2);
            
            auto toggleWidth { 140 }, toggleHeight { 22 };
            phaseBox.setSize(toggleWidth, toggleHeight);
            phaseBox.setCentrePosition(getWidth()/2, h - 2 * toggleHeight);
            
            lowKnob.setSize(toggleWidth/3, toggleWidth/3);
            highKnob.setSize(toggleWidth/3, toggleWidth/3);
            auto topCentreOfButton { phaseBox.getBounds().getCentre().withY(phaseBox.getBounds().getY()) };
            lowKnob.setTopRightPosition(topCentreOfButton.x, topCentreOfButton.y - toggleWidth/3);
            highKnob.setTopLeftPosition(topCentreOfButton.x, topCentreOfButton.y - toggleWidth/3);
        }
        void mouseDown(const MouseEvent& e) override { if (e.mods.isRightButtonDown()) { popupMenu.show(this); } }
        
        struct RightClickMenu : PopupMenu {
            RightClickMenu() {
                
                PopupMenu::Item itemLoad { "Show Info On Mouse-Over" };
                itemLoad.action = { [&] { toggleMouseOverEnabled(); } };
                addItem(itemLoad);
                PopupMenu::Item itemSave { "Show Filter Controls" };
                itemSave.action = { [&,this] { toggleFilterControls(); } };
                addItem(itemSave);
                PopupMenu::Item itemLoadWaveTable { "Load Wave Table" };
                itemLoadWaveTable.action = { [&,this] { loadFile(); } };
                addItem(itemLoadWaveTable);
            }
            void show(Component* comp) {
                PopupMenu::Options options;
                PopupMenu::showMenuAsync(options.withTargetComponent(comp)); }
            std::function<void()> toggleMouseOverEnabled, toggleFilterControls;
            std::unique_ptr<FileChooser> fc;
            std::function<void()> loadFile, saveFile;
            std::function<void(int)> selectTable;
            std::function<void (const FileChooser&)> loadCallback, saveCallback;
        };
        
        WaveTableManager& mgmt;
        EmpiricalLAF elaf;
        FilterLAF alaf;
        SadTextButton button;
        SadBox phaseBox { "PHASE TABLES", mgmt };
        SadSVG driveSVG { Data::DRIVE_svg, Colours::grey }, deviationSVG { Data::DEVIATION_svg, Colours::grey };
        TransLabel valueLabel, suffixLabel, lLabel, rLabel, lSuffix, rSuffix;
        int currentEffect;
        EmpiricalSlider driveKnob { true }, deviateKnob;
        FilterKnob lowKnob { Data::apiKnob2_svg }, highKnob { Data::apiKnob2_svg };
        DeviantSlider blendKnob;
        bool mouseOverActive { false }, filterControlsActive { false };
        RightClickMenu popupMenu;
        std::unique_ptr<APVTS::SliderAttachment> driveAttachment, lowAttachment, highAttachment, deviationAttachment, blendAttachment;
    };
    
    template<typename TableType> struct MoreDials : Component, Timer {
        void timerCallback() override {
            if (mgmt.mgr.newGUIDataHere) {
                pad.repaint();
                shouldRepaintAgain = true;
                mgmt.mgr.newGUIDataHere = false;
            }
            else if (shouldRepaintAgain) {
                pad.repaint();
                shouldRepaintAgain = false;
            }
        }
        
        static constexpr int numSliders { 12 }, numPanels { 4 };
        struct Panel : Component {
            Panel(int n, EmpiricalSlider* s, EmpiricalSlider* bS) : num(n) {
                for (int i { 0 }; i < num; ++i) {
                    sliders[i] = &s[i];
                    addAndMakeVisible(sliders[i]);
                }
                sliders[num] = bS;
                addAndMakeVisible(sliders[num]);
            }
            void resized() override {
                const auto localBounds { getLocalBounds() };
                Rectangle<int> bounds[4];
                bounds[0] = getLocalBounds();
                int removal = bounds[0].getHeight()/(num+1);
                for (int i = 1; i <= num; ++i) bounds[i] = bounds[0].removeFromBottom(removal);
                for (int i { 0 }; i <= num; ++i) {
                    if(sliders[i]->isLeft) sliders[i]->setBounds(bounds[i]);
                    else sliders[i]->setBounds(bounds[i]);
                    sliders[i]->setTextBoxStyle(sliders[i]->isLeft ? Slider::TextBoxLeft : Slider::TextBoxRight, false, localBounds.getHeight()/num, localBounds.getHeight()/num);
                }
            }
            int num;
            EmpiricalSlider* sliders[4] { nullptr };
        };
        MoreDials(WaveTableManager& t, float* g, float* p, const float& m) : mgmt(t), pad(g, p, m) {
            
            for(int j { 0 }; j < numPanels; ++j) {
                for(int i { 0 }; i <= panel[j].num; ++i) {
                    if(j < 2) panel[j].sliders[i]->setLookAndFeel(&lelaf);
                    else panel[j].sliders[i]->setLookAndFeel(&relaf);
                    panel[j].sliders[i]->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
                    if(j > 1) {
                        if(!panel[j].sliders[i]->isDefaultHigh) panel[j].sliders[i]->setRotaryParameters(0.0f, 5.81333f, true);
                        else panel[j].sliders[i]->setRotaryParameters(5.81333f, 0.0f, true);
                    }
                    else {
                        if(panel[j].sliders[i]->isDefaultHigh) panel[j].sliders[i]->setRotaryParameters(degreesToRadians(513.f), degreesToRadians(180.f), true);
                        else panel[j].sliders[i]->setRotaryParameters(degreesToRadians(180.f), degreesToRadians(513.f), true);
                    }
                    panel[j].sliders[i]->setDoubleClickReturnValue(true, 1.0f, ModifierKeys::altModifier);
                    panel[j].sliders[i]->setMouseDragSensitivity (50);
                    panel[j].sliders[i]->setNumDecimalPlacesToDisplay(2);
                    addAndMakeVisible(panel[j]);
                }
                addAndMakeVisible(panel[j]);
            }
            addAndMakeVisible(frame);

            if (pad.table.type.contains("dynamic")) {
                phaseBox.setSelectedId(int(*mgmt.apvts.getRawParameterValue("dynamicWaveShaperTableID")));
                phaseBox.setText("PHASE TABLES");
                phaseBox.loadCallback = [&,this] (const FileChooser& chooser) {
                    if (!chooser.getResults().isEmpty()) {
                        if (mgmt.loadTable(chooser.getResult()))
                            juce::AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Table Loaded", "!");
                        else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not loaded correctly", "!");
                    }
                    else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not choosen correctly", "!");
                };
//                phaseBox.loadFile = [&,this](){
//                    popupMenu.fc = std::make_unique<FileChooser>("Load Wave Table (.wav)", File(), "*.wav");
//                    popupMenu.fc->launchAsync (FileBrowserComponent::openMode |
//                                               FileBrowserComponent::canSelectFiles,
//                                               popupMenu.loadCallback);
//                };
                addAndMakeVisible(phaseBox);
            }
            addAndMakeVisible(pad);
            addAllAndMakeVisible(*this, xAxisLabel, yAxisLabel, button);
            button.label.set(pad.table.buttonString, pad.table.textColour, pad.table.bgColour);
        }
        
        void resized() override {
            Rectangle<int> middle { getLocalBounds() };
            Rectangle<int> bounds[4];
            bounds[0] = middle.removeFromLeft(getWidth()/8);
            bounds[1] = bounds[0].removeFromTop(getHeight()/2);
            bounds[2] = middle.removeFromRight(getWidth()/8);
            bounds[3] = bounds[2].removeFromTop(getHeight()/2);
            for(int j { 0 }; j < numPanels; ++j) panel[j].setBounds(bounds[j]);
            frame.setBounds(getLocalBounds().reduced(getLocalBounds().getWidth()/8, 0));
            button.setBounds(middle.getX() + middle.getWidth() - 105, middle.getY() + 5, 95, 15);
            xAxisLabel.setBounds(middle.getX(), middle.getY() + 9 * middle.getHeight()/10, middle.getWidth(), middle.getHeight()/10);
            yAxisLabel.setBounds(middle.getX(), middle.getY(), middle.getHeight()/10, middle.getHeight());
            pad.setBounds(middle.reduced(20));
            phaseBox.setBounds(middle.getX() + 5,middle.getY() + 5, 150, 20);
        }
        WaveTableManager& mgmt;
        Component frame;
        WavePad<TableType> pad;
        LeftEmpiricalLAF lelaf;
        RightEmpiricalLAF relaf;
        Image   volume, filter, in, out, hz, dB;
        sadistic::TransLabel valueLabel, suffixLabel;
        EmpiricalSlider knobs[numSliders] { { true, true }, { true, true }, { false, true }, { false, true }, { true, true }, { true, true, true }, { false, true }, { false, true, true }, { true, true, true }, { false, true, true }, { true, true, true }, { false, true, true } };
        APVTS::SliderAttachment attachments[numSliders] {
            { mgmt.apvts, { pad.table.type +    "AtanDrive" },              knobs[0] },
            { mgmt.apvts, { pad.table.type +    "HyperbolicBlend" },        knobs[1] },
            { mgmt.apvts, { pad.table.type +    "HyperbolicDrive" },        knobs[2] },
            { mgmt.apvts, { pad.table.type +    "BitCrusherDrive" },        knobs[3] },
            { mgmt.apvts, { pad.table.type +    "LogisticDrive" },         knobs[4] },
            { mgmt.apvts, { pad.table.type +    "ClipperDrive" },           knobs[5] },
            { mgmt.apvts, { pad.table.type +    "ClipperBlend" },           knobs[6] },
            { mgmt.apvts, { pad.table.type +    "AtanBlend" },              knobs[7] },
            { mgmt.apvts, { pad.table.type +    "BitCrusherBlend" },        knobs[8] },
            { mgmt.apvts, { pad.table.type +    "AtanBlend" },              knobs[9] },
            { mgmt.apvts, { pad.table.type +    "BitCrusherBlend" },        knobs[10] },
            { mgmt.apvts, { pad.table.type +    "LogisticBlend" },         knobs[11] }
        };
        Panel panel[numPanels] { { 2, &knobs[4], &knobs[10] }, { 2, &knobs[0], &knobs[8] }, { 2, &knobs[6], &knobs[11] }, { 2, &knobs[2], &knobs[9] } };
        
        SadLabel xAxisLabel { pad.table.xString, false, 0.f };
        SadLabel yAxisLabel { pad.table.yString, true, -0.5f };
        SadTextButton button { pad.table.buttonString };
        SadBox phaseBox { "PHASE TABLES", mgmt };
        APVTS::ComboBoxAttachment boxAttachment { mgmt.apvts, "dynamicWaveShaperTableID", phaseBox };
        bool shouldRepaintAgain { false };
    };
}
