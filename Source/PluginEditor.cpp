#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace sadistic;

sadistic::WTFEditor::WTFEditor (WTF& p) : AudioProcessorEditor (&p), wtf (p) {
    EmpiricalLAF::setDefaultLookAndFeel(&llaf);
    guiIdx = wtf.getCurrentScreen();
    setWantsKeyboardFocus(true);
    setInterceptsMouseClicks(false, true);
    addAndMakeVisible(renderer);
    activateScreen(guiIdx);
    renderer.addAndMakeVisible(toggleButtonLeft);
    renderer.addAndMakeVisible(toggleButtonRight);

//    toggleButtonLeft.onClick = [&] {
//        int idx { ((guiIdx - 1) + numProDisplays) % numProDisplays };
//        if (guiIdx == dynamicPad) idx = dials;
//        switchScreen(idx); };
//    toggleButtonRight.onClick = [&] {
//        int idx { (guiIdx + 1) % numProDisplays };
//        if (guiIdx == staticPad) idx = matrix;
//        switchScreen(idx); };
    
    toggleButtonLeft.onClick = [&] { switchScreen(guiIdx == dynamicPad ? dials : (((guiIdx - 1) + numProDisplays) % numProDisplays)); };
    toggleButtonRight.onClick = [&]{ switchScreen(guiIdx == staticPad ? matrix : ((guiIdx + 1) % numProDisplays)); };
    
    theMoreDials.button.onClick = [this] { switchScreen(dynamicPad); };
    phaseDials.button.onClick = [this] { switchScreen(staticPad); };
    
//    if(!wtf.marketplaceStatus.isUnlocked()) {
//        authorizer = std::make_unique<SadisticUnlockForm>(wtf.marketplaceStatus);
//        renderer.addAndMakeVisible(*authorizer);
//        startTimer(1000);
//    }
    
    scope.prepare({ wtf.getSampleRate(), (uint32) 2, (uint32) 1 });
    
    setResizeLimits(150, 75, 1980, 1080);
    setResizable(true, false);
    setSize (600, 300);
}

sadistic::WTFEditor::~WTFEditor(){
    EmpiricalLAF::setDefaultLookAndFeel(nullptr);
    if (guiIdx == staticPad) theMoreDials.stopTimer();
    if (guiIdx == dynamicPad) phaseDials.stopTimer();
    stopTimer();
}

void sadistic::WTFEditor::switchScreen(int idx){
    if (idx == staticPad) theMoreDials.stopTimer();
    if (idx == dynamicPad) phaseDials.stopTimer();
    screen[guiIdx]->setVisible(false);
    renderer.removeChildComponent(screen[guiIdx]);
    activateScreen(guiIdx = idx);
}

void sadistic::WTFEditor::activateScreen(int idx){
    wtf.setCurrentScreen(idx);
    renderer.addAndMakeVisible(screen[guiIdx]);
    if (idx == staticPad) theMoreDials.startTimerHz(40);
    if (idx == dynamicPad) phaseDials.startTimerHz(40);
    
    // these need to always be in front to be clickable after the display changes
    toggleButtonLeft.toFront(false);
    toggleButtonRight.toFront(false);
    renderer.repaint();
}

void sadistic::WTFEditor::timerCallback() {
//    if(wtf.marketplaceStatus.isUnlocked()) {
//        stopTimer();
//        if(authorizer) authorizer.reset();
//    }
}

void sadistic::WTFEditor::resized() {
    auto bounds { getBounds() };
    renderer.setBounds(bounds);
    auto toggleWidth { jmax(10,getWidth()/20) }, toggleHeight { jmax(20,bounds.getHeight()/20) };
    toggleButtonRight.setBounds(bounds.getCentreX() + 10, bounds.getY() + toggleHeight, toggleWidth, toggleHeight);
    toggleButtonLeft.setBounds(bounds.getCentreX() - (10 + toggleWidth), bounds.getY() + toggleHeight, toggleWidth, toggleHeight);
    for (auto* s : screen) s->setBounds(getBounds());
//    if(authorizer) authorizer->setBounds(bounds);
}

// this function is declared in the PluginProcessor.h and defined here to keep the editor out of the processor translation unit
AudioProcessorEditor* sadistic::createWTFEditor(WTF& deviant) { return new sadistic::WTFEditor (deviant); }
