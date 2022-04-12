#pragma once
#include "../../Deviant/Source/Members.h"
#include "deviantPro.h"
#include "../../Source/SadisticFIR.h"

namespace sadistic {

    template<typename F>
    struct SadisticFilter : public DeviantEffect {
        SadisticFilter(String eID, ParamList refs, FloatParamList floatRefs, int eIDX) : DeviantEffect(eID, refs, floatRefs, eIDX) {}
        template<typename OtherFloatType> SadisticFilter(SadisticFilter<OtherFloatType>& other) : DeviantEffect(other) {}
        void reset() override { filter.reset(); }
        int getLatency() const override { return static_cast<int>(filter.state->getFilterOrder()/2); }
        void prepare(const ProcessSpec& spec) override {
            lastSampleRate = jlimit(44100.0, 192000.0, spec.sampleRate);
            filter.prepare({ lastSampleRate, spec.maximumBlockSize, spec.numChannels }); }
        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
            if constexpr (std::is_same<F, FloatType>::value) {
                AudioBlock<F> block { buffer };
                filter.process(ProcessContextReplacing<F>(block));
            }
        }
        void calculateCoefficients() override {
            auto& [low, high, noName2, noName3, noName4, noName5, attenuation, blend] = coeffs;
            update(lastSampleRate, low, high); }
        void update(double sR, F lo, F hi){
            *filter.state = *makeBandpass<F>(lo, hi, sR, FIXEDFILTERORDER, WindowingFunction<F>::kaiser, F(4)); }
        double lastSampleRate { 44100.0 };
        ProcessorDuplicator<FIR::Filter<F>, FIR::Coefficients<F>> filter;
    };
    
    template<typename F> struct DynamicWaveShaper : DeviantEffect {

        static constexpr int bufferLength { BUFFERLENGTH }, maxWaveOrder { 12 }, xMaxLength { 16 }, maxWavelength { 1 << maxWaveOrder }, fifoLength { maxWavelength + bufferLength }, waveLength { WAVELENGTH }, gainLength { GAINLENGTH }, filterOrder { FILTERORDER };
        
        DynamicWaveShaper(String eID, ParamList refs, FloatParamList floatRefs, int eIDX, std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs], WaveTableManager& m) :
        DeviantEffect(eID, refs, floatRefs, eIDX), mgmt(m), coeffIdx(cI), mCoeffs(cS) {}
        DynamicWaveShaper(DynamicWaveShaper<float>& o) : DeviantEffect(o), mgmt(o.mgmt), coeffIdx(o.coeffIdx), mCoeffs(o.mCoeffs) {}
        DynamicWaveShaper(DynamicWaveShaper<double>& o) : DeviantEffect(o), mgmt(o.mgmt), coeffIdx(o.coeffIdx), mCoeffs(o.mCoeffs) {}
        
        static constexpr int distance(int older, int newer) { return ((newer-older) + fifoLength) % fifoLength; }
        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
        void prepare(const ProcessSpec& spec) override { for (auto& w : waveShaper) { w.lastSampleRate = jlimit(44100.0, 192000.0, spec.sampleRate); w.spectralInversionDelay.prepare(spec); w.spectralInversionDelay2.prepare(spec);  } reset(); }
        void reset() override { for (auto& w : waveShaper) { w.reset(); } }
        int getLatency() const override { return fifoLength + waveShaper[0].preFilter.getLatency() + waveShaper[0].postFilter.getLatency(); }
        
        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
            if constexpr (std::is_same<F, FloatType>::value) {
                if (mgmt.newPhaseTableSetAvailable) {
                    phaseTableSet.copyTableData(mgmt.phaseTableSet);
                    mgmt.newPhaseTableSetAvailable = false;
                }
                float pre { static_cast<AudioParameterFloat*>(mgmt.apvts.getParameter("dynamicWaveShaperPreCutoff"))->get() }, post { static_cast<AudioParameterFloat*>(mgmt.apvts.getParameter("dynamicWaveShaperPostCutoff"))->get() };
                if (pre != lastPreCutoff) {
                    auto state = makeZoom<F>(F(20), lastPreCutoff = jlimit(lastPreCutoff/maxMult, lastPreCutoff * maxMult, pre), waveShaper[0].lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                    for (int j { 0 }; j < buffer.getNumChannels(); ++j)
                        waveShaper[j].preFilter.setCoefficients(state.getRawCoefficients());
                }
                if (post != lastPostCutoff) {
                    auto state = makeZoom<F>(F(20), lastPostCutoff = jlimit(lastPostCutoff/maxMult, lastPostCutoff * maxMult, post), waveShaper[0].lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                    for (int j { 0 }; j < buffer.getNumChannels(); ++j)
                        waveShaper[j].postFilter.setCoefficients(state.getRawCoefficients());
                }
                for (int j { 0 }; j < buffer.getNumChannels(); ++j)
                    waveShaper[j].process(buffer.getWritePointer(j));
                print(octaves);
            }
        }
        
        struct WaveShaper {
            using Comparator = bool(WaveShaper::*)(const int);
            using TurnFunction = void(WaveShaper::*)(const int);
            
            WaveShaper(DynamicWaveShaper& wS) : shaper(wS) {
                auto preState = makeZoom<F>(F(20), F(2000), lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                preFilter.setCoefficients(preState.getRawCoefficients());
                auto postState = makeZoom<F>(F(20), F(10000), lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                preFilter.setCoefficients(postState.getRawCoefficients());
            }
            
            void reset() {
                compare = &WaveShaper::isTrough;
                compareOther = &WaveShaper::isPeak;
                turn = &WaveShaper::turnTrough;
                turnOther = &WaveShaper::turnPeak;
                waveIndex = cross = cross2 = 0;
                peak = trough = crossing = secondCrossing = -1;
                slope = crossingSlope = zero<F>;
                std::fill(wave, wave + fifoLength, zero<F>);
                std::fill(spectralInversionBuffer, spectralInversionBuffer + bufferLength, zero<F>);
                spectralInversionDelay.reset();
                spectralInversionDelay2.reset();
                preFilter.reset();
            }
            
            void process (F* buffer) {
                spectralInversionDelay.processOneChannelOnly(buffer, spectralInversionBuffer, bufferLength);
                preFilter.process(bufferLength, buffer);
                FloatVectorOperations::subtract(spectralInversionBuffer, buffer, bufferLength);
                spectralInversionDelay2.processOneChannelOnly(spectralInversionBuffer, bufferLength);
                
                for (int i { 0 }; i < bufferLength; ++i) wave[waveIndex + i] = buffer[i];
                for(int i { waveIndex }, end { waveIndex + bufferLength }; i < end; ++i) {
                    if((this->*compare)(i)) {
                        (this->*turn)(leftOf(i));
                        switchDirections();
                    }
                }
                waveIndex = (waveIndex + bufferLength) % fifoLength;
                if (crossing >= waveIndex && crossing < waveIndex + bufferLength) crossing = -1;
                for (int i { 0 }; i < bufferLength; ++i) buffer[i] = wave[waveIndex + i];
                
                postFilter.process(bufferLength, buffer);
                
                FloatVectorOperations::add(buffer, spectralInversionBuffer, bufferLength);
            }
            
            F getDC(int i, int length, F startDC, F endDC) { return getHalfCosineValue(i, length, startDC, endDC); }
            void switchDirections() { std::swap(compare, compareOther); std::swap(turn, turnOther); }
            int leftN(int i, int N) { return ((i - N) + fifoLength) % fifoLength; }
            int rightN(int i, int N) { return (i + N) % fifoLength; }
            int leftOf(int i) { return ((i - 1) + fifoLength) % fifoLength; }
            int rightOf(int i) { return (i + 1) % fifoLength; }
            
            bool isTrough (const int i) {
                F valLeft { wave[leftOf(i)] }, val { wave[i] };
                F localSlope { valLeft - val };
                if (localSlope - slope > -slopeTolerance) {
                    if (localSlope - slope > slopeTolerance) {
                        cross2 = cross = leftOf(i);
                        slope = localSlope;
                    }
                    else cross2 = leftOf(i);
                }
                return valLeft < val;
            }
            
            bool isPeak (const int i) {
                F valLeft { wave[leftOf(i)] }, val { wave[i] };
                F localSlope { val - valLeft };
                if (localSlope - slope > -slopeTolerance) {
                    if (localSlope - slope > slopeTolerance) {
                        cross2 = cross = leftOf(i);
                        slope = localSlope;
                    }
                    else cross2 = leftOf(i);
                }
                return val < valLeft;
            }
            
            void turnPeak(const int newPeak) {
                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
                auto newCrossingSlope { getSlopeAt(newCrossing) };
                if (crossing != -1) convolve(newCrossing, newCrossingSlope);
                crossing = newCrossing;
                crossingSlope = newCrossingSlope;
                peak = newPeak;
                slope = wave[peak] - wave[rightOf(peak)];
            }
            
            void turnTrough(const int newTrough) {
                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
                trough = newTrough;
                secondCrossing = newCrossing;
                slope = wave[rightOf(trough)] - wave[trough];
            }

            F getSlopeAt(int x) { return (wave[rightOf(x)] - wave[leftOf(x)])/two<F>; }
            F getAmp(F s, int length) {
                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length, 1) };
                F returnValue { abs(s/normalSineAmplitude) };
//                jassert(returnValue < F(1.5));
                return jlimit(zero<F>, one<F>, returnValue);
            }

            void convolve(int newCrossing, F newCrossingSlope) {
                const auto blend { static_cast<F>(shaper.getBlend()) };
                const int start { crossing }, end { newCrossing }, length { distance(start, end) };
                if (length < 4) return;

                int leftIndex { 0 };
                F framePos { zero<F> };
                shaper.phaseTableSet.getSetPosition(shaper.coeffs[1], leftIndex, framePos);
                auto octave { shaper.phaseTableSet.getOctave(length) };
                F phaseStep { one<F> / F(length) }, phase { zero<F> };
                F startAmp { getAmp(crossingSlope, length) }, endAmp { getAmp(newCrossingSlope, length) };
                F ampStep { (endAmp - startAmp) / F(length) }, amp { startAmp }, startDC { wave[start] }, endDC { wave[end] };
                
                // CONVOLVE TABLE
                for (int i { start }, j { 0 }; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    auto waveSample { wave[i] };
                    wave[i] = blend * (getDC(j, length, startDC, endDC) + wtSample) + (one<F> - blend) * waveSample;//wave[i];
                    jassert(!isnan(wave[i]));
                    jassert(abs(wave[i]) < F(1.5));
                }
                shaper.octaves[octave] += 1;
            }
            
            F wave[fifoLength]{}, spectralInversionBuffer[bufferLength]{};
            Comparator compare { &WaveShaper::isTrough }, compareOther { &WaveShaper::isPeak };
            TurnFunction turn { &WaveShaper::turnTrough }, turnOther { &WaveShaper::turnPeak };
            int waveIndex { 0 }, cross { 0 }, cross2 { 0 }, peak { -1 }, trough { -1 }, crossing { -1 }, secondCrossing { -1 };
            F slope { zero<F> }, crossingSlope { zero<F> };
            const F slopeTolerance { static_cast<F>(0.0001) };
            DynamicWaveShaper& shaper;
            IntelSingleRateFIR<F, filterOrder> preFilter, postFilter;
            SadDelay<F, filterOrder/2> spectralInversionDelay;
            SadDelay<F, fifoLength + filterOrder/2> spectralInversionDelay2;
            double lastSampleRate { 44100.0 };
        };
        
        F pWave[waveLength + 1]{}, makeup { one<F> };
        WaveTableManager& mgmt;
        WaveShaper waveShaper[2] { { *this }, { *this } };
        PhaseTableSet<F> phaseTableSet;
        float lastPreCutoff { 1.f }, lastPostCutoff { 1.f }, maxMult { 1.05f };
        std::atomic<int>* coeffIdx;
        float(& mCoeffs)[numFX][maxCoeffs][maxCoeffs];
        float mWaveTablePosition { 0.f };
        bool goingUp { true };
        int octaves[NUMOCTAVES]{};
    };
    
    
//    template<typename F> struct DynamicWaveShaper : DeviantEffect, ValueTree::Listener {
//
//        static constexpr int bufferLength { BUFFERLENGTH }, maxWaveOrder { 12 }, maxWavelength { 1 << maxWaveOrder }, fifoLength { maxWavelength + bufferLength }, waveLength { WAVELENGTH }, gainLength { GAINLENGTH };
//        static constexpr F zero { static_cast<F>(0) }, one { static_cast<F>(1) }, two { static_cast<F>(2) }, half { one / two }, quarter { half / two }, pi { MathConstants<F>::pi }, halfPi { MathConstants<F>::halfPi }, twoPi { MathConstants<F>::twoPi };
//
//        DynamicWaveShaper(String eID, ParamList refs, FloatParamList floatRefs, int eIDX, WaveTableManager& s, std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) :
//        DeviantEffect(eID, refs, floatRefs, eIDX), mgmt(s), coeffIdx(cI), mCoeffs(cS) {}
//        template<typename OtherFloatType> DynamicWaveShaper(DynamicWaveShaper<OtherFloatType>& other) : DeviantEffect(other), mgmt(other.mgmt), coeffIdx(other.coeffIdx), mCoeffs(other.mCoeffs) {}
//
//        static constexpr int distance(const int older, const int newer) { return ((newer-older) + fifoLength) % fifoLength; }
//        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
//        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
//        void prepare(const ProcessSpec&) override { reset(); }
//        void reset() override { for (auto& w : waveShaper) w.reset(); }
//        int getLatency() const override { return fifoLength; }
//        struct WaveShaper {
//            using Comparator = bool(WaveShaper::*)(const int);
//            using TurnFunction = void(WaveShaper::*)(const int);
//
//            WaveShaper(DynamicWaveShaper& wS) : shaper(wS) {}
//
//            void reset() {
//                compare = &WaveShaper::isTrough;
//                compareOther = &WaveShaper::isPeak;
//                turn = &WaveShaper::turnTrough;
//                turnOther = &WaveShaper::turnPeak;
//                waveIndex = cross = cross2 = 0;
//                peak = trough = crossing = secondCrossing = -1;
//                slope = crossingSlope = zero;
//                std::fill(wave, wave + fifoLength, zero); }
//
//            void process (F* buffer) {
//                for (int i { 0 }; i < bufferLength; ++i) wave[waveIndex + i] = buffer[i];
//                for(int i { waveIndex }, end { waveIndex + bufferLength }; i < end; ++i) {
//                    if((this->*compare)(i)) {
//                        (this->*turn)(leftOf(i));
//                        switchDirections();
//                    }
//                }
//                waveIndex = (waveIndex + bufferLength) % fifoLength;
//                if (crossing >= waveIndex && crossing < waveIndex + bufferLength) crossing = -1;
//                for (int i { 0 }; i < bufferLength; ++i) buffer[i] = wave[waveIndex + i];
//            }
//            F getDC(int i, int length, F startDC, F endDC) {
//                return startDC + (endDC - startDC) * (half - std::cos((F(i) * pi)/F(length))/two); }
//            void switchDirections() { std::swap(compare, compareOther); std::swap(turn, turnOther); }
//            int leftOf(int i) { return ((i - 1) + fifoLength) % fifoLength; }
//            int rightOf(int i) { return (i + 1) % fifoLength; }
//
//            bool isTrough (const int i) {
//                F valLeft { wave[leftOf(i)] }, val { wave[i] };
//                F localSlope { valLeft - val };
//                if (localSlope - slope > -slopeTolerance) {
//                    if (localSlope - slope > slopeTolerance) {
//                        cross2 = cross = leftOf(i);
//                        slope = localSlope;
//                    }
//                    else cross2 = leftOf(i);
//                }
//                return valLeft < val;
//            }
//
//            bool isPeak (const int i) {
//                F valLeft { wave[leftOf(i)] }, val { wave[i] };
//                F localSlope { val - valLeft };
//                if (localSlope - slope > -slopeTolerance) {
//                    if (localSlope - slope > slopeTolerance) {
//                        cross2 = cross = leftOf(i);
//                        slope = localSlope;
//                    }
//                    else cross2 = leftOf(i);
//                }
//                return val < valLeft;
//            }
//
//            void turnPeak(const int newPeak) {
//                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
//                auto newCrossingSlope { getSlopeAt(newCrossing) };
//                if (crossing != -1) convolve(newCrossing, newPeak, newCrossingSlope);
//                crossing = newCrossing;
//                crossingSlope = newCrossingSlope;
//                peak = newPeak;
//                slope = zero;
//            }
//
//            void turnTrough(const int newTrough) {
//                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
//                trough = newTrough;
//                secondCrossing = newCrossing;
//                slope = zero;
//            }
//
//            F getAmplitudeExiting(int x, int nextCrossing) {
//                int length { distance(x, nextCrossing) };
//                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
//                F dcAtAdjacentSample { wave[x] - (wave[x] - wave[nextCrossing])/F(length) };
//                return jmin(one, abs((wave[rightOf(x)] - dcAtAdjacentSample) / normalSineAmplitude));
//            }
//
//            F getAmplitudeEntering(int x, int lastCrossing) {
//                int length { distance(lastCrossing, x) };
//                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
//                F dcAtAdjacentSample { wave[x] - (wave[x] - wave[lastCrossing])/F(length) };
//                return jmin(one, abs((wave[leftOf(x)] - dcAtAdjacentSample) / normalSineAmplitude));
//            }
//
//            F getSlopeAt(int x) { return (wave[rightOf(x)] - wave[leftOf(x)])/two; }
//            F getAmpFromSlopeAndLength(F s, int length) {
//                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
//                return abs(s) / normalSineAmplitude;
//            }
//
//            void convolve(int newCrossing, int newPeak, F newCrossingSlope) {
//                const int length1 { distance(crossing, secondCrossing) }, length2 { distance(secondCrossing, newCrossing) };
//                if(length1 > 1 && length2 > 1 && distance(peak, trough) > 1 && distance(trough, newPeak) > 1) {
//                    F secondSlope { getSlopeAt(secondCrossing) };
//                    F amp1 { getAmpFromSlopeAndLength(crossingSlope, length1) };
//                    F amp2 { getAmpFromSlopeAndLength(secondSlope, length1) };
//                    F amp3 { getAmpFromSlopeAndLength(secondSlope, length2) };
//                    F amp4 { getAmpFromSlopeAndLength(newCrossingSlope, length2) };
//                    //                    F amp1 { getAmplitudeExiting(crossing, secondCrossing) };
//                    //                    F amp2 { getAmplitudeEntering(secondCrossing, crossing) };
//                    //                    F amp3 { getAmplitudeExiting(secondCrossing, newCrossing) };
//                    //                    F amp4 { getAmplitudeEntering(newCrossing, secondCrossing) };
//                    //                    const auto& table { shaper.phaseTableSet->getTable(length1 + length2) };
//                    const auto blend { static_cast<F>(shaper.getBlend()) };
//                    const auto& table { shaper.phaseTable };
//                    const auto& aCoeffs { shaper.mCoeffs[0][int(shaper.coeffIdx[0])] };
//                    const auto& bCoeffs { shaper.mCoeffs[1][int(shaper.coeffIdx[1])] };
//                    const auto& dCoeffs { shaper.mCoeffs[2][int(shaper.coeffIdx[2])] };
//                    convolve(crossing, secondCrossing, zero, half, amp1, amp2, blend, table, aCoeffs, bCoeffs, dCoeffs);
//                    convolve(secondCrossing, newCrossing, half, one, amp3, amp4, blend, table, aCoeffs, bCoeffs, dCoeffs);
//                }
//            }
//            void convolve (int start, int end, F startPhase, F endPhase, F startAmp, F endAmp, F blend, const Table<F>& table, const float (&a)[maxCoeffs], const float (&b)[maxCoeffs], const float (&d)[maxCoeffs]) {
//                auto length { distance(start, end) };
//                F phaseStep { (endPhase - startPhase) / F(length) };
//                F dcStep { (wave[end] - wave[start]) / F(length) };
//                F ampStep { (endAmp - startAmp) / F(length) };
//                F phase { startPhase }, dc { wave[start] }, amp { startAmp };
//
//                for (int i { start }, j { 0 }; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
//                    F phaseAmplitude { table[phase] };
////                    F multipliedWithGain { amp * shapeSample(a, b, d, phaseAmplitude) };
//                    F multipliedWithGain { amp * phaseAmplitude };
//                    wave[i] = blend * (getDC(j, length, wave[start], wave[end]) + multipliedWithGain) + (one - blend) * wave[i];
//                }
//            }
//            F wave[fifoLength]{};
//            Comparator compare { &WaveShaper::isTrough }, compareOther { &WaveShaper::isPeak };
//            TurnFunction turn { &WaveShaper::turnTrough }, turnOther { &WaveShaper::turnPeak };
//            int waveIndex { 0 }, peak { -1 }, trough { -1 }, crossing { -1 }, secondCrossing { -1 }, cross { 0 }, cross2 { 0 };
//            F slope { zero }, slopeTolerance { static_cast<F>(0.0001) }, crossingSlope { zero };
//            DynamicWaveShaper& shaper;
//        };
//
//        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
//            if constexpr (std::is_same<F, FloatType>::value) {
//
//                if (mgmt.newPhaseTableSetAvailable)
//                    for (int i { 0 }; i < waveLength + 1; ++i)
//                        pWave[i] = static_cast<F>(mgmt.phaseTableSet.tableData[i]);
//
//                for (int j { 0 }; j < buffer.getNumChannels(); ++j)
//                    waveShaper[j].process(buffer.getWritePointer(j));
//            }
//        }
//
//        WaveTableManager& mgmt;
//        F pWave[waveLength + 1]{};
//        //        Table<F> gainTable { wave, static_cast<F>(gainLength), half, half };
//        PhaseTableSet<F> phaseTableSet;
//        Table<F> phaseTable { pWave, static_cast<F>(waveLength) };
//        WaveShaper waveShaper[2] { { *this }, { *this } };
//        std::atomic<int>* coeffIdx;
//        float(& mCoeffs)[numFX][maxCoeffs][maxCoeffs];
//    };
}
