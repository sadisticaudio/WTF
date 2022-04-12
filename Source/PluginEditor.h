#pragma once
#include "wtRenderer.h"
#include "../../Deviant/Source/Analyzer.h"
#include "wtAnalyzer.h"
#include "DialsPro.h"
#include "RoutingMatrix.h"
namespace sadistic {
    
    class WTFEditor : public AudioProcessorEditor, private Timer {
        static constexpr int scopeSize { SCOPESIZE };
    public:
        WTFEditor(WTF&);
        ~WTFEditor() override;
        void resized() override;
        void activateScreen(int);
        void switchScreen(int);
        void timerCallback() override;
        
    private:
        WTF& wtf;
        EmpiricalLAF llaf;
        int guiIdx { dials }, dialMode { 0 };
        ScopeBuffer scopeBuffer[numSignals];
        
        
        WaveTableBuffer waveTableBuffer;
        WaveTableAnalyzer waveTableAnalyzer { wtf.getWaveTableManager(), waveTableBuffer };
        
        
        WaveTableRenderer renderer { wtf.getAPVTS(), scopeBuffer, waveTableBuffer };
        DualScope scope { wtf.getOscilloscopeFifo(), scopeBuffer };
        Dials2 theDials { wtf.getWaveTableManager(), dialMode };
        MoreDials<GUITable> theMoreDials { wtf.getWaveTableManager(), wtf.getTableManager().inputTable, wtf.getTableManager().gainTable, wtf.getTableManager().waveMult };
        MoreDials<DualGUITable> phaseDials { wtf.getWaveTableManager(), wtf.getWaveTableManager().phaseTable, wtf.getWaveTableManager().gainTable, wtf.getWaveTableManager().gainMult };
        RoutingMatrix routingMatrix { wtf.getWaveTableManager() };
        Component* screen[numProDisplays] { &theDials, &theMoreDials, &phaseDials, &routingMatrix };
        SadButton toggleButtonLeft { true }, toggleButtonRight { false };
        std::unique_ptr<SadisticUnlockForm> authorizer;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WTFEditor)
    };
}
