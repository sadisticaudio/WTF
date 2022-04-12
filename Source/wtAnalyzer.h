#pragma once
#include "deviantPro.h"

namespace sadistic {
    
    class WaveTableAnalyzer : private Timer {
    public:
        static constexpr int fifoSize { FIFOSIZE }, scopeSize { SCOPESIZE }, wtResolution { WTRESOLUTION };
        WaveTableAnalyzer(WaveTableManager& m, WaveTableBuffer& w) : mgmt(m), waveTableBuffer(w) { startTimerHz (30); }

        void timerCallback() override {
            if (mgmt.guiNewPhaseTableSetAvailable) {
                PhaseTableSet<float>& set { mgmt.phaseTableSet };
                float* buf { waveTableBuffer.getBlankFrame() };
                if (buf) {
                    for (int p { 0 }; p < waveTableBuffer.wtResolution; ++p) {
                        int leftIndex { 0 };
                        float framePos { zero<float> };
                        set.getSetPosition(float(p)/static_cast<float>(waveTableBuffer.wtResolution), leftIndex, framePos);
                        float phaseStep { one<float> / static_cast<float>(waveTableBuffer.wtResolution) }, phase { zero<float> };
                        for (int i { 0 }; i < waveTableBuffer.wtResolution; ++i, phase += phaseStep) {
                            buf[p * wtResolution + i] = set.getSample(0, leftIndex, framePos, phase);
                        }
                    }
                    waveTableBuffer.setReadyToRender();
                }
            }
        }
        
    private:
        WaveTableManager& mgmt;
        WaveTableBuffer& waveTableBuffer;
    };
} // namespace sadistic

