#pragma once
#include "deviantPro.h"
namespace sadistic {
    
    struct GUITable {
        GUITable(float* i, float* g, const float& m) : inputTable { i, float(SCOPESIZE) }, outputTable { g, float(SCOPESIZE), 0.5f, 0.5f }, mult(m) {}
        float operator[](int idx) const { return outputTable[inputTable[idx]] / mult; }
        float operator[](float norm) const { return outputTable[inputTable[norm]] / mult; }
        Table<float> inputTable, outputTable;
        const float& mult;
        const String type { "static" }, xString { "A M P L I T U D E   I N" }, yString { "A M P L I T U D E   O U T" }, buttonString { "S T A T I C" };
        const Colour textColour { Colours::black }, bgColour { Colours::grey.darker() };
    };
    
    struct DualGUITable {
        DualGUITable(float* p, float* g, const float& m) : phaseTable { p, float(SCOPESIZE) }, gainTable { g, float(SCOPESIZE), 0.5f, 0.5f }, mult { m } {}
        float operator[](float norm) const { return gainTable[phaseTable[norm]] / mult; }
        Table<float> phaseTable, gainTable;
        const float& mult;
        const String type { "dynamic" }, xString { "P   H   A   S   E" }, yString { "A M P L I T U D E" }, buttonString { "D Y N A M I C" };
        const Colour textColour { Colours::white }, bgColour { Colours::blue.darker() };
    };
    
    template <typename TableType> struct WavePad : public Component {
        static constexpr int scopeSize { SCOPESIZE };
        WavePad(float* g, float* p, const float& m) : table(g, p, m) {
            setInterceptsMouseClicks(true, true);
            wavePath.preallocateSpace(scopeSize * 4); }
        float gainToY(float gain) { auto h { float(getHeight()) }; return h/2 - gain * (h/2) * 0.707f; }
        float indexToX(int i) { return 15.f + (float(i)/float(scopeSize)) * (getLocalBounds().toFloat().getWidth() - 25.f); }
        void paint (Graphics& g) override {
            bool thereAreIdiotsAmongstUs { false };
            wavePath.clear();
            const auto firstX { table[0] };
//            jassert(!isnan(firstX));
            if (!isnan(firstX)) wavePath.startNewSubPath(indexToX(0), gainToY(table[0]));
            else thereAreIdiotsAmongstUs = true;
            float norm { 0.f };
            for (int i { 0 }; i <= scopeSize; ++i, norm += 1.f/float(scopeSize)) {
                const auto x { table[norm] };
//                jassert(!isnan(x));
                if (!isnan(x)) wavePath.lineTo(indexToX(i), gainToY(table[norm]));
                else thereAreIdiotsAmongstUs = true;
            }
            g.setColour(Colours::white.darker());
            if (!thereAreIdiotsAmongstUs) g.strokePath(wavePath, PathStrokeType(5.f));
        }
        TableType table;
        Path wavePath;
    };
}
