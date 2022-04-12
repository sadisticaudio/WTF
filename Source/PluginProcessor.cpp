#include "PluginProcessor.h"
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new sadistic::WTF({}); }

sadistic::WTF::WTF(AudioProcessorValueTreeState::ParameterLayout layout) : AudioProcessor (getDefaultBusesProperties()), membersD(layout, mgmt, cIdx, coefficients), membersF(membersD), mgmt(apvts, &undoManager, staticIdx, staticCoefficients, cIdx, coefficients), apvts(*this, &undoManager, "PARAMETERS", std::move(layout)) {
    apvts.state.setProperty (Identifier("currentScreen"), var(int(0)), nullptr);
//    marketplaceStatus.load();
//    String waveTablePath { getSadisticFolderPath() + "/Wave Tables/Stock/MATRIXYC64.wav" };
    apvts.state.setProperty(Identifier("waveTableFile"), var("Stock/MATRIXYC64.wav"), &undoManager);
    membersF.init();
    membersD.init();
}

AudioProcessorEditor* sadistic::WTF::createEditor() { return createWTFEditor(*this); }
