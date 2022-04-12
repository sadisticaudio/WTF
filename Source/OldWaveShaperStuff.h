#pragma once
#include "../../Deviant/Source/Members.h"
#include "deviantPro.h"
#include "../../Source/SadisticFIR.h"

namespace sadistic {
    
    
    
    static void makeMAYBESineToTriangleWaveTableFile() {
        static constexpr int numFrames { 12 }, waveLength { WAVELENGTH };
        AudioBuffer<float> buf;
        buf.setSize(1, (numFrames) * waveLength + 1);
        buf.clear();
        float phaseStep { twoPi<float>/float(waveLength) };
        int startFrame { 0 };
        int harmonics[numFrames] { 1,2,3,4,6,8,11,16,23,32,46,64 };
        
        //    while (startFrame < numFrames) {
        for (int f { startFrame }; f < numFrames; ++f) {
            for ( int harmonic { 1 }, h { 0 }; h < harmonics[f]; ++h, harmonic+=2) {
                auto* data { buf.getWritePointer(0, f * waveLength) };
                float phase { zero<F> }, iteration { one<F> };
                //                for(int iteration { 1 }, harmonic { 1 }; harmonic < 5; iteration *= -1, harmonic += 2)
                //                    for(int i { 0 }; i < static_cast<int>(length * numCycles); ++i, phase+=phaseStep)
                //                        table[i] += iteration * (one<F>/F(harmonic * harmonic)) * sin(phase * harmonic);
                for (int i { 0 }; i < waveLength; ++i, phase += phaseStep)
                    data[i] += iteration * (one<F>/F(harmonic * harmonic)) * sin(phase * harmonic);
            }
        }
        //        startFrame++;
        //    }
        //    for (int i { 0 }; i < waveLength; ++i)
        //        buf.setSample(0, i, buf.getSample(0, waveLength + i));
        //    buf.setSize(1, numFrames * waveLength + 1, true, false, true);
        File file { "~/Desktop/example.wav" };
        saveTable(file, buf);
    }
    
    static void makeSineToSawWaveTableFile() {
        static constexpr int numFrames { 12 }, waveLength { WAVELENGTH };
        AudioBuffer<float> buf;
        buf.setSize(1, (numFrames) * waveLength + 1);
        buf.clear();
        float phaseStep { twoPi<float>/float(waveLength) };
        int harmonics[numFrames] { 1,2,3,4,6,8,11,16,23,32,46,64 };
        
        auto* data { buf.getWritePointer(0) };
        
        for (int f { 0 }; f < numFrames; ++f) {
            for ( int harmonic { 1 }, h { 0 }; h < harmonics[f]; ++h, harmonic++) {
                float phase { zero<F> };
                for (int i { 0 }; i < waveLength; ++i, phase += phaseStep)
                    data[f * waveLength + i] += (one<F>/F(harmonic)) * sin(phase * F(harmonic));
            }
            auto mag { Wave<float>::getMagnitude(buf.getReadPointer(0, f * waveLength), waveLength) };
            for (int i { 0 }; i < waveLength; ++i)
                data[f * waveLength + i] *= (one<F>/mag);
        }
        
        File file { "~/Desktop/example.wav" };
        saveTable(file, buf);
    }
    
    template<typename F>
    struct OldDynamicWaveShaper : DeviantEffect {
        
        static constexpr int bufferLength { BUFFERLENGTH }, maxWaveOrder { 12 }, xMaxLength { 16 }, maxWavelength { 1 << maxWaveOrder }, fifoLength { maxWavelength + bufferLength }, waveLength { WAVELENGTH }, gainLength { GAINLENGTH }, filterOrder { FILTERORDER };
        
        OldDynamicWaveShaper(String eID, ParamList refs, FloatParamList floatRefs, int eIDX, WaveTableManager& m, std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) :
        DeviantEffect(eID, refs, floatRefs, eIDX), mgmt(m), coeffIdx(cI), mCoeffs(cS) {}
        template<typename OtherFloatType> OldDynamicWaveShaper(OldDynamicWaveShaper<OtherFloatType>& other) : DeviantEffect(other), mgmt(other.mgmt), coeffIdx(other.coeffIdx), mCoeffs(other.mCoeffs) {}
        
        static constexpr int distance(const int older, const int newer) { return ((newer-older) + fifoLength) % fifoLength; }
        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
        void prepare(const ProcessSpec& spec) override { for (auto& w : waveShaper) { w.lastSampleRate = jlimit(44100.0, 192000.0, spec.sampleRate); w.spectralInversionDelay.prepare(spec); } reset(); }
        void reset() override { for (auto& w : waveShaper) { w.reset(); } }
        int getLatency() const override { return fifoLength + waveShaper[0].filter.getLatency(); }
        
        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
            if constexpr (std::is_same<F, FloatType>::value) {
                if (mgmt.newPhaseTableSetAvailable) {
                    phaseTableSet.copyTableData(mgmt.phaseTableSet);
                    mgmt.newPhaseTableSetAvailable = false;
                }
                for (int j { 0 }; j < buffer.getNumChannels(); ++j)
                    waveShaper[j].process(buffer.getWritePointer(j));
            }
        }
        
        struct WaveShaper {
            using Comparator = bool(WaveShaper::*)(const int);
            using TurnFunction = void(WaveShaper::*)(const int);
            
            WaveShaper(OldDynamicWaveShaper& wS) : shaper(wS) {
                auto state = makeZoom<F>(F(20), F(2000), lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                filter.setCoefficients(state.getRawCoefficients());
                //                auto* state = *makeBandpass<double>(20.0, 2000.0, lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
                //                filter.setCoefficients(state->getRawCoefficients());
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
                std::fill(xWave, xWave + xMaxLength, zero<F>);
                spectralInversionDelay.reset();
                
                filter.reset();
            }
            
            void process (F* buffer) {
                
                //                if (buffer[0] != zero<F>)
                //                    samplesProcessed += bufferLength;
                
                spectralInversionDelay.processOneChannelOnly(buffer, spectralInversionBuffer, bufferLength);
                filter.process(bufferLength, buffer);
                FloatVectorOperations::subtract(spectralInversionBuffer, buffer, bufferLength);
                
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
                
                FloatVectorOperations::add(buffer, spectralInversionBuffer, bufferLength);
                
                if (peak != -1 && trough != -1 && crossing != -1) {
                    std::cout << "\n";
                    print("samples before:");
                    print(peakSamplesBefore);
                    print("samples after:");
                    print(peakSamplesAfter);
                }
                
                //                auto diff { samplesProcessed - samplesConvolved };
                //                if (diff != differenceTally) {
                //                    std::cout << "diff: " << diff << "\n";
                //                    differenceTally = diff;
                //                }
            }
            
            F getDC(int i, int length, F startDC, F endDC) {
                return startDC + (endDC - startDC) * (half<F> - std::cos((F(jmin(i,length)) * pi<F>)/F(length))/two<F>); }
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
                if (crossing != -1) convolve(newCrossing, newPeak, newCrossingSlope);
                crossing = newCrossing;
                crossingSlope = newCrossingSlope;
                peak = newPeak;
                slope = zero<F>;
            }
            
            void turnTrough(const int newTrough) {
                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
                trough = newTrough;
                secondCrossing = newCrossing;
                slope = zero<F>;
            }
            
            //            F getAmplitudeExiting(int x, int nextCrossing) {
            //                int length { distance(x, nextCrossing) };
            //                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
            //                F dcAtAdjacentSample { wave[x] - (wave[x] - wave[nextCrossing])/F(length) };
            //                return jmin(one<F>, abs((wave[rightOf(x)] - dcAtAdjacentSample) / normalSineAmplitude));
            //            }
            //
            //            F getAmplitudeEntering(int x, int lastCrossing) {
            //                int length { distance(lastCrossing, x) };
            //                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
            //                F dcAtAdjacentSample { wave[x] - (wave[x] - wave[lastCrossing])/F(length) };
            //                return jmin(one<F>, abs((wave[leftOf(x)] - dcAtAdjacentSample) / normalSineAmplitude));
            //            }
            
            F getSlopeAt(int x) { return (wave[rightOf(x)] - wave[leftOf(x)])/two<F>; }
            F getAmpFromSlopeAndLength(F s, int length) {
                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
                return abs(s) / normalSineAmplitude;
            }
            F getAmpFromSlopeAndFullLength(F s, int length) {
                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length, 1) };
                return abs(s) / normalSineAmplitude;
            }
            
            void convolve(int newCrossing, int newPeak, F newCrossingSlope) {
                const int length { distance(crossing, newCrossing) }, length1 { distance(crossing, secondCrossing) }, length2 { distance(secondCrossing, newCrossing) };
                if(length1 > 2 && length2 > 2 && distance(peak, trough) > 2 && distance(trough, newPeak) > 2) {
                    samplesConvolved += (length1 + length2);
                    
                    F secondSlope { getSlopeAt(secondCrossing) };
                    F amp1 { getAmpFromSlopeAndFullLength(crossingSlope, length1 + length2) };
                    F amp4 { getAmpFromSlopeAndFullLength(newCrossingSlope, length1 + length2) };
                    F midAmp { amp1 + (amp4 - amp1)/2 };
                    
                    auto wtPos { shaper.coeffs[1] };
                    unsigned int leftIndex { 0 };
                    F framePos { 0 };
                    shaper.phaseTableSet.getSetPosition(wtPos, leftIndex, framePos);
                    //                    auto octave { shaper.phaseTableSet.getOctave(jmin(length1, length2) * 2) };
                    auto octave { shaper.phaseTableSet.getOctave(length) };
                    //                    convolve1(crossing, secondCrossing, newCrossing, zero<F>, half<F>, amp1, midAmp, octave, leftIndex, framePos);
                    //                    convolve2(secondCrossing, newCrossing, newCrossing, half<F>, one<F>, midAmp, amp4, octave, leftIndex, framePos);
                    convolve(crossing, newCrossing, amp1, amp4, octave, leftIndex, framePos);
                }
            }
            
            template <int N> void getNSamplesAroundIndex(F (& dst)[N], int idx) {
                for (int i { leftN(idx, N/2) }, j { 0 }; j < N; ++j, ++i%=fifoLength) dst[j] = wave[i]; }
            
            void convolve (int start, int end, F startAmp, F endAmp, int octave, unsigned int leftIndex, F framePos) {
                if ((distance(crossing, peak) > 8) && (distance(peak, trough) > 8))
                    getNSamplesAroundIndex(peakSamplesBefore, crossing);
                
                const auto blend { static_cast<F>(shaper.getBlend()) };
                
                auto firstProjectedSample { xWave[0] };
                auto startingWtSample { startAmp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, zero<F>) };
                auto startDCCorrection { startingWtSample - firstProjectedSample };
                auto startDC { startDCCorrection }, endDC { wave[end] };
                
                auto length { distance(start, end) };
                F phaseStep { one<F> / F(length) };
                F dcStep { -startDCCorrection / F(length) };
                F ampStep { (endAmp - startAmp) / F(length) };
                F phase { zero<F> }, dc { startDCCorrection }, amp { startAmp };
                int i { start }, j { 0 }, xLength { jmin(xMaxLength, length) };
                
                // CROSSFADE FROM PROJECTED/EXTENDED SAMPLES
                for (; j < xLength; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    auto sampleWithDC { getDC(j, length, wave[start], wave[end]) + dc + wtSample };
                    auto fadedSample { sampleWithDC * (F(j)/F(xLength)) + xWave[j] * (F(xLength - j)/F(xLength)) };
                    wave[i] = blend * fadedSample + (one<F> - blend) * wave[i];
                    jassert(!isnan(wave[i]));
                }
                
                // CONVOLVE TABLE
                for (; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    wave[i] = blend * (getDC(j, length, wave[start], wave[end]) + dc + wtSample) + (one<F> - blend) * wave[i];
                    jassert(!isnan(wave[i]));
                }
                
                int numFrames { shaper.phaseTableSet.getNumFrames() };
                unsigned int newLeftIndex { static_cast<unsigned int>(jmin((int)leftIndex + 1, numFrames - 1)) };
                phase = zero<F>;
                phaseStep = jmin(phaseStep, one<F>/F(xMaxLength));
                
                // FILL XFADE BUFFER
                for (int k { 0 }; k < xMaxLength; ++k, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, newLeftIndex, framePos, phase) };
                    xWave[k] = wave[end] + wtSample;
                    jassert(!isnan(xWave[k]));
                }
            }
            
            void convolve1 (int start, int end, int newCrossing, F startPhase, F endPhase, F startAmp, F endAmp, int octave, unsigned int leftIndex, F framePos) {
                if ((distance(crossing, peak) > 8) && (distance(peak, trough) > 8)) {
                    
                    getNSamplesAroundIndex(peakSamplesBefore, crossing);
                }
                
                int totalLength { distance(crossing, newCrossing) };
                
                const auto blend { static_cast<F>(shaper.getBlend()) };
                
                auto length { distance(start, end) };
                F phaseStep { (endPhase - startPhase) / F(length) };
                F dcStep { (wave[end] - wave[start]) / F(length) };
                F ampStep { (endAmp - startAmp) / F(length) };
                F phase { startPhase }, dc { wave[start] }, amp { startAmp };
                int i { start }, j { 0 }, xLength { jmin(xMaxLength, length) };
                
                // CROSSFADE FROM PREVIOUS TABLE
                for (; j < xLength; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    auto sampleWithDC { getDC(j, totalLength, wave[crossing], wave[newCrossing]) + wtSample };
                    auto fadedSample { sampleWithDC * (F(j)/F(xLength)) + xWave[j] * (F(xLength - j)/F(xLength)) };
                    wave[i] = blend * fadedSample + (one<F> - blend) * wave[i];
                    jassert(!isnan(wave[i]));
                }
                
                // CONVOLVE TABLE
                for (; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    wave[i] = blend * (getDC(j, totalLength, wave[crossing], wave[newCrossing]) + wtSample) + (one<F> - blend) * wave[i];
                    jassert(!isnan(wave[i]));
                }
                
                if ((distance(crossing, peak) > 8) && (distance(peak, trough) > 8))
                    getNSamplesAroundIndex(peakSamplesAfter, crossing);
            }
            
            void convolve2 (int start, int end, int newCrossing, F startPhase, F endPhase, F startAmp, F endAmp, int octave, unsigned int leftIndex, F framePos) {
                
                int totalLength { distance(crossing, newCrossing) };
                const auto blend { static_cast<F>(shaper.getBlend()) };
                
                auto length { distance(start, end) };
                F phaseStep { (endPhase - startPhase) / F(length) };
                F dcStep { (wave[end] - wave[start]) / F(length) };
                F ampStep { (endAmp - startAmp) / F(length) };
                F phase { startPhase }, dc { wave[start] }, amp { startAmp };
                int i { start }, j { distance(crossing, secondCrossing) };
                
                // CONVOLVE TABLE
                for (; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
                    wave[i] = blend * (getDC(j, totalLength, wave[crossing], wave[newCrossing]) + wtSample) + (one<F> - blend) * wave[i];
                    jassert(!isnan(wave[i]));
                }
                
                int numFrames { shaper.phaseTableSet.getNumFrames() };
                unsigned int newLeftIndex { static_cast<unsigned int>(jmin((int)leftIndex + 1, numFrames - 1)) };
                phase = zero<F>;
                phaseStep = jmin(phaseStep, one<F>/F(xMaxLength));
                
                // FILL XFADE BUFFER
                for (int k { 0 }; k < xMaxLength; ++j, ++k, phase += phaseStep, amp += ampStep, dc += dcStep) {
                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, newLeftIndex, framePos, phase) };
                    xWave[k] = wave[end] + wtSample;
                    jassert(!isnan(xWave[k]));
                }
            }
            
            F wave[fifoLength]{}, xWave[xMaxLength]{}, spectralInversionBuffer[bufferLength]{}, peakSamplesBefore[16]{}, peakSamplesAfter[16]{};
            Comparator compare { &WaveShaper::isTrough }, compareOther { &WaveShaper::isPeak };
            TurnFunction turn { &WaveShaper::turnTrough }, turnOther { &WaveShaper::turnPeak };
            int waveIndex { 0 }, cross { 0 }, cross2 { 0 }, peak { -1 }, trough { -1 }, crossing { -1 }, secondCrossing { -1 };
            F slope { zero<F> }, crossingSlope { zero<F> };
            size_t samplesProcessed { 0 }, samplesConvolved { 0 }, differenceTally { 0 };
            const F slopeTolerance { static_cast<F>(0.0001) };
            OldDynamicWaveShaper& shaper;
            IntelSingleRateFIR<F, filterOrder> filter;
            SadDelay<F, filterOrder/2> spectralInversionDelay;
            double lastSampleRate { 44100.0 };
        };
        
        F pWave[waveLength + 1]{}, makeup { one<F> };
        WaveTableManager& mgmt;
        WaveShaper waveShaper[2] { { *this }, { *this } };
        PhaseTableSet<F> phaseTableSet;
        
        std::atomic<int>* coeffIdx;
        float(& mCoeffs)[numFX][maxCoeffs][maxCoeffs];
        float mWaveTablePosition { 0.f };
        bool goingUp { true };
    };
    
    
}



// WAVESHAPER BEFORE GUTTING SOME NON-WORKING XFADE 16 SAMPLE AND OTHER STUFF

//#pragma once
//#include "../../Deviant/Source/Members.h"
//#include "deviantPro.h"
//#include "../../Source/SadisticFIR.h"
//
//namespace sadistic {
//
//    template<typename F>
//    struct SadisticFilter : public DeviantEffect {
//        SadisticFilter(String eID, ParamList refs, FloatParamList floatRefs, int eIDX) : DeviantEffect(eID, refs, floatRefs, eIDX) {}
//        template<typename OtherFloatType> SadisticFilter(SadisticFilter<OtherFloatType>& other) : DeviantEffect(other) {}
//        void reset() override { filter.reset(); }
//        int getLatency() const override { return static_cast<int>(filter.state->getFilterOrder()/2); }
//        void prepare(const ProcessSpec& spec) override {
//            lastSampleRate = jlimit(44100.0, 192000.0, spec.sampleRate);
//            filter.prepare({ lastSampleRate, spec.maximumBlockSize, spec.numChannels }); }
//        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
//        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
//        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
//            if constexpr (std::is_same<F, FloatType>::value) {
//                AudioBlock<F> block { buffer };
//                filter.process(ProcessContextReplacing<F>(block));
//            }
//        }
//        void calculateCoefficients() override {
//            auto& [low, high, noName2, noName3, noName4, noName5, attenuation, blend] = coeffs;
//            update(lastSampleRate, low, high); }
//        void update(double sR, F lo, F hi){
//            *filter.state = *makeBandpass<F>(lo, hi, sR, FIXEDFILTERORDER, WindowingFunction<F>::kaiser, F(4)); }
//        double lastSampleRate { 44100.0 };
//        ProcessorDuplicator<FIR::Filter<F>, FIR::Coefficients<F>> filter;
//    };
//
//    template<typename F>
//    struct DynamicWaveShaper : DeviantEffect {
//
//        static constexpr int bufferLength { BUFFERLENGTH }, maxWaveOrder { 12 }, xMaxLength { 16 }, maxWavelength { 1 << maxWaveOrder }, fifoLength { maxWavelength + bufferLength }, waveLength { WAVELENGTH }, gainLength { GAINLENGTH }, filterOrder { FILTERORDER };
//
//        DynamicWaveShaper(String eID, ParamList refs, FloatParamList floatRefs, int eIDX, WaveTableManager& m, std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) :
//        DeviantEffect(eID, refs, floatRefs, eIDX), mgmt(m), coeffIdx(cI), mCoeffs(cS) {}
//        template<typename OtherFloatType> DynamicWaveShaper(DynamicWaveShaper<OtherFloatType>& other) : DeviantEffect(other), mgmt(other.mgmt), coeffIdx(other.coeffIdx), mCoeffs(other.mCoeffs) {}
//
//        static constexpr int distance(const int older, const int newer) { return ((newer-older) + fifoLength) % fifoLength; }
//        void processSamples(AudioBuffer<float>& buffer) override { processSamples<float>(buffer); }
//        void processSamples(AudioBuffer<double>& buffer) override { processSamples<double>(buffer); }
//        void prepare(const ProcessSpec& spec) override { for (auto& w : waveShaper) { w.lastSampleRate = jlimit(44100.0, 192000.0, spec.sampleRate); w.spectralInversionDelay.prepare(spec); w.spectralInversionDelay2.prepare(spec);  } reset(); }
//        void reset() override { for (auto& w : waveShaper) { w.reset(); } }
//        int getLatency() const override { return fifoLength + waveShaper[0].preFilter.getLatency() + waveShaper[0].postFilter.getLatency(); }
//
//        template<typename FloatType> void processSamples(AudioBuffer<FloatType>& buffer) {
//            if constexpr (std::is_same<F, FloatType>::value) {
//                if (mgmt.newPhaseTableSetAvailable) {
//                    phaseTableSet.copyTableData(mgmt.phaseTableSet);
//                    mgmt.newPhaseTableSetAvailable = false;
//                }
//                float pre { static_cast<AudioParameterFloat*>(mgmt.apvts.getParameter("dynamicWaveShaperPreCutoff"))->get() }, post { static_cast<AudioParameterFloat*>(mgmt.apvts.getParameter("dynamicWaveShaperPostCutoff"))->get() };
//                if (pre != lastPreCutoff) {
//                    auto state = makeZoom<F>(F(20), lastPreCutoff = jlimit(lastPreCutoff/maxMult, lastPreCutoff * maxMult, pre), waveShaper[0].lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
//                    for (int j { 0 }; j < buffer.getNumChannels(); ++j)
//                        waveShaper[j].preFilter.setCoefficients(state.getRawCoefficients());
//                }
//                if (post != lastPostCutoff) {
//                    auto state = makeZoom<F>(F(20), lastPostCutoff = jlimit(lastPostCutoff/maxMult, lastPostCutoff * maxMult, post), waveShaper[0].lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
//                    for (int j { 0 }; j < buffer.getNumChannels(); ++j)
//                        waveShaper[j].postFilter.setCoefficients(state.getRawCoefficients());
//                }
//                for (int j { 0 }; j < buffer.getNumChannels(); ++j)
//                    waveShaper[j].process(buffer.getWritePointer(j));
//            }
//        }
//
//        struct WaveShaper {
//            using Comparator = bool(WaveShaper::*)(const int);
//            using TurnFunction = void(WaveShaper::*)(const int);
//
//            WaveShaper(DynamicWaveShaper& wS) : shaper(wS) {
//                auto preState = makeZoom<F>(F(20), F(2000), lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
//                preFilter.setCoefficients(preState.getRawCoefficients());
//                auto postState = makeZoom<F>(F(20), F(10000), lastSampleRate, filterOrder, WindowingFunction<F>::kaiser, 4.0);
//                preFilter.setCoefficients(postState.getRawCoefficients());
//            }
//
//            void reset() {
//                compare = &WaveShaper::isTrough;
//                compareOther = &WaveShaper::isPeak;
//                turn = &WaveShaper::turnTrough;
//                turnOther = &WaveShaper::turnPeak;
//                waveIndex = cross = cross2 = 0;
//                peak = trough = crossing = secondCrossing = expectedCrossing = -1;
//                slope = crossingSlope = zero<F>;
//                std::fill(wave, wave + fifoLength, zero<F>);
//                std::fill(xWave, xWave + xMaxLength, zero<F>);
//                std::fill(spectralInversionBuffer, spectralInversionBuffer + bufferLength, zero<F>);
//                spectralInversionDelay.reset();
//                spectralInversionDelay2.reset();
//                preFilter.reset();
//            }
//
//            void process (F* buffer) {
//                spectralInversionDelay.processOneChannelOnly(buffer, spectralInversionBuffer, bufferLength);
//                preFilter.process(bufferLength, buffer);
//                FloatVectorOperations::subtract(spectralInversionBuffer, buffer, bufferLength);
//                spectralInversionDelay2.processOneChannelOnly(spectralInversionBuffer, bufferLength);
//
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
//
//                postFilter.process(bufferLength, buffer);
//
//                FloatVectorOperations::add(buffer, spectralInversionBuffer, bufferLength);
//            }
//
//            F getDC(int i, int length, F startDC, F endDC) { return getHalfCosineValue(i, length, startDC, endDC); }
//            void switchDirections() { std::swap(compare, compareOther); std::swap(turn, turnOther); }
//            int leftN(int i, int N) { return ((i - N) + fifoLength) % fifoLength; }
//            int rightN(int i, int N) { return (i + N) % fifoLength; }
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
//                if (crossing != expectedCrossing) {
//                    auto lastSample { wave[leftOf(crossing)] }, secondToLastSample { wave[leftOf(leftOf(crossing))] };
//                    auto projectedStart { lastSample + (lastSample - secondToLastSample) };
//                    auto dcStep { -projectedStart/F(xMaxLength) }, dc { projectedStart };
//                    for (int k { crossing }, h { 0 }; h < xMaxLength; ++h, ++k%=fifoLength, dc += dcStep)
//                        xWave[h] = dc + wave[k];
//                }
//                if (crossing != -1) convolve(newCrossing, newCrossingSlope);
//                crossing = newCrossing;
//                crossingSlope = newCrossingSlope;
//                peak = newPeak;
//                //                slope = zero<F>;
//                slope = wave[peak] - wave[rightOf(peak)];
//            }
//
//            void turnTrough(const int newTrough) {
//                int newCrossing { (cross + distance(cross, cross2)/2) % fifoLength };
//                trough = newTrough;
//                secondCrossing = newCrossing;
//                //                slope = zero<F>;
//                slope = wave[rightOf(trough)] - wave[trough];
//            }
//
//            F getSlopeAt(int x) { return (wave[rightOf(x)] - wave[leftOf(x)])/two<F>; }
//            F getAmpFromSlopeAndLength(F s, int length) {
//                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length * 2, 1) };
//                return abs(s) / normalSineAmplitude;
//            }
//            // NOTE: LOOK INTO WHY THIS OUTPUTS AN EXTREME VALUE WHEN LENGTH IS SMALL (LENGTH = 2, VALUE RETURNED IS SOMETHING LIKE -84563543.253)!  THIS WAS TEMPORARILY FIXED BY A jlimit(zero<F>, one<F>, value) in the convolve function
//            F getAmp(F s, int length) {
//                F normalSineAmplitude { Wave<F>::getSineAmplitudeAtIndex(length, 1) };
//                F returnValue { abs(s/normalSineAmplitude) };
//                jassert(returnValue < F(1.5));
//                return jlimit(zero<F>, one<F>, returnValue);
//            }
//            F getAmpOfShortWave(int start, int length) {
//                F lowerBound { wave[start] }, upperBound { wave[start] };
//                for (int i { rightOf(start) }, j { 1 }; j < length; ++j, ++i%=fifoLength) {
//                    if (wave[i] < lowerBound) lowerBound = wave[i];
//                    if (wave[i] > upperBound) upperBound = wave[i];
//                }
//                F returnValue { abs(upperBound - lowerBound)/two<F> };
//                jassert(returnValue < one<F>);
//                return jlimit(zero<F>, one<F>, returnValue);
//            }
//
//            float getLevel(F pk, F trgh) {
//                return static_cast<float>(jlimit(zero<F>, one<F>, (one<F> - pow((F(100) + Decibels::gainToDecibels(sqrt(jlimit(zero<F>, one<F>, (pk - trgh) * two<F>))))/F(100), 2.f) - F(0.1)) * two<F>));
//            }
//
//            void convolve(int newCrossing, F newCrossingSlope) {
//                const int start { crossing }, end { newCrossing }, length { distance(start, end) };
//                if (length < 2) return;
//                F startAmp { zero<F> }, endAmp { zero<F> };
//                if (length > 4) {
//                    startAmp = getAmp(crossingSlope, length);
//                    endAmp = getAmp(newCrossingSlope, length);
//                }
//                else startAmp = endAmp = getAmpOfShortWave(crossing, length);
//
//                int leftIndex { 0 };
//                F framePos { zero<F> };
//                auto wtPos { shaper.coeffs[1] };
//                //                auto wtPos { getLevel(wave[peak], wave[trough]) };
//                shaper.phaseTableSet.getSetPosition(wtPos, leftIndex, framePos);
//                auto octave { shaper.phaseTableSet.getOctave(length) };
//                const auto blend { static_cast<F>(shaper.getBlend()) };
//                //                auto firstProjectedSample { xWave[0] };
//                //                auto startingWtSample { startAmp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, zero<F>) };
//                //                auto startDCCorrection { startingWtSample - firstProjectedSample };
//                //                F dcStep { -startDCCorrection / F(length) }, dc { startDCCorrection };
//                //                jassert(abs(dc) < F(0.01));
//                F dcStep { zero<F> }, dc { zero<F> };
//                F phaseStep { one<F> / F(length) }, phase { zero<F> };
//                F ampStep { (endAmp - startAmp) / F(length) }, amp { startAmp };
//                int i { start }, j { 0 }, xLength { jmin(xMaxLength, length) };
//
//                //                // CROSSFADE FROM PROJECTED/EXTENDED SAMPLES
//                //                for (; j < xLength; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
//                //                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
//                //                    auto sampleWithDC { getDC(j, length, wave[start], wave[end]) + dc + wtSample };
//                //                    auto fadedSample { sampleWithDC * (F(j)/F(xLength)) + xWave[j] * (F(xLength - j)/F(xLength)) };
//                //                    wave[i] = blend * fadedSample + (one<F> - blend) * wave[i];
//                //                    jassert(!isnan(wave[i]));
//                //                    jassert(abs(wave[i]) < F(1.5));
//                //                }
//
//                F startDC { wave[start] }, endDC { wave[end] };
//
//                // CONVOLVE TABLE
//                for (; i != end; ++i%=fifoLength, ++j, phase += phaseStep, amp += ampStep, dc += dcStep) {
//
//                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, leftIndex, framePos, phase) };
//                    auto waveSample { wave[i] };
//                    wave[i] = blend * (getDC(j, length, startDC, endDC) + wtSample) + (one<F> - blend) * waveSample;//wave[i];
//                    jassert(!isnan(wave[i]));
//                    jassert(abs(wave[i]) < F(1.5));
//                }
//
//                //                int numFrames { shaper.phaseTableSet.getNumFrames() };
//                //                int newLeftIndex { jmin((int)leftIndex + 1, numFrames - 1) };
//                //                phase = zero<F>;
//                //                phaseStep = jmin(phaseStep, one<F>/F(xMaxLength));
//                //
//                //                // FILL XFADE BUFFER
//                //                for (int k { 0 }; k < xMaxLength; ++k, phase += phaseStep, amp += ampStep, dc += dcStep) {
//                //                    auto wtSample { amp * shaper.phaseTableSet.getSample(octave, newLeftIndex, framePos, phase) };
//                //                    xWave[k] = wave[end] + wtSample;
//                //                    jassert(!isnan(xWave[k]));
//                //                }
//                //                expectedCrossing = newCrossing;
//            }
//
//            F wave[fifoLength]{}, spectralInversionBuffer[bufferLength]{}, xWave[xMaxLength]{};
//            Comparator compare { &WaveShaper::isTrough }, compareOther { &WaveShaper::isPeak };
//            TurnFunction turn { &WaveShaper::turnTrough }, turnOther { &WaveShaper::turnPeak };
//            int waveIndex { 0 }, cross { 0 }, cross2 { 0 }, peak { -1 }, trough { -1 }, crossing { -1 }, secondCrossing { -1 }, expectedCrossing { -1 };
//            F slope { zero<F> }, crossingSlope { zero<F> };
//            const F slopeTolerance { static_cast<F>(0.0001) };
//            DynamicWaveShaper& shaper;
//            IntelSingleRateFIR<F, filterOrder> preFilter, postFilter;
//            SadDelay<F, filterOrder/2> spectralInversionDelay;
//            SadDelay<F, fifoLength + filterOrder/2> spectralInversionDelay2;
//            double lastSampleRate { 44100.0 };
//        };
//
//        F pWave[waveLength + 1]{}, makeup { one<F> };
//        WaveTableManager& mgmt;
//        WaveShaper waveShaper[2] { { *this }, { *this } };
//        PhaseTableSet<F> phaseTableSet;
//        float lastPreCutoff { 1.f }, lastPostCutoff { 1.f }, maxMult { 1.05f };
//        std::atomic<int>* coeffIdx;
//        float(& mCoeffs)[numFX][maxCoeffs][maxCoeffs];
//        float mWaveTablePosition { 0.f };
//        bool goingUp { true };
//    };
//}




// PHASESHAPER WORKING BEFORE GUTTING SOME NON-WORKING snapToZeroCrossing related stuff

//#pragma once
//#include "../../Source/sadistic.h"
//
//namespace sadistic {
//
//    enum { WAVELENGTH = 2048 };
//
//    template<typename F> struct Table {
//        F operator[](int idx) const { return table[idx]; }
//        F operator[](F sample) const {
//            F floatIndex { jlimit(zero<F>, one<F>, slope * sample + intercept) * waveLength };
//            auto i { truncatePositiveToUnsignedInt (floatIndex) };
//            F f { floatIndex - F (i) };
//            F x0 { table[i] }, x1 { table[i + 1] };
//            return jmap (f, x0, x1); }
//        F* table { nullptr };
//        F waveLength { WAVELENGTH }, slope { one<F> }, intercept { zero<F> };
//    };
//
//    template <typename F> static void xFadeEdgesToSine(F* wave, int length, int fadeLength) {
//        std::vector<F> sinBegin, sinEnd;
//        for (int i { 0 }; i < fadeLength; ++i) {
//            auto sinB { sin((F(i) * twoPi<F>)/F(length)) };
//            auto sinE { sin((F(-i) * twoPi<F>)/F(length)) };
//            auto ratio { F(i)/F(fadeLength) };
//            sinBegin.push_back(sinB);
//            //            print("sample: ", i, " - BEFORE: ", wave[i]);
//            auto b { wave[i] };
//            auto e { wave[length - i] };
//            auto bFinal { wave[i] * ratio + sinB * (one<F> - ratio) };
//            auto eFinal { wave[length - i] * ratio + sinE * (one<F> - ratio) };
//            wave[i] = wave[i] * ratio + sinB * (one<F> - ratio);
//            //            print("sample: ", i, " - AFTER:  ", wave[i]);
//            if (i != 0) {
//
//
//                wave[length - i] = wave[length - i] * ratio + sinE * (one<F> - ratio);
//            }
//        }
//    }
//
//    template<typename F> struct PhaseTableSet {
//        static constexpr int waveLength { WAVELENGTH }, numOctaves { NUMOCTAVES }, maxFrames { 256 }, maxLength { maxFrames * waveLength };
//
//        int getNumFrames() const { return numFrames; }
//        int getFrameLength(int octave) const { return waveLength >> octave; }
//        int getOctave(int len) const { int i = waveLength, o = 0; for (; i > len; i /= 2, ++o); return jlimit(0, numOctaves - 1, o); }
//        void getSetPosition(const float wtPos, int& leftIndex, F& framePos) const {
//            F floatIndex { jlimit(0.f, 1.f, wtPos) * float(numFrames - 1) };
//            leftIndex = jlimit(0, numFrames - 1, (int)truncatePositiveToUnsignedInt (floatIndex));
//            framePos = floatIndex - F (leftIndex);
//            jassert (isPositiveAndNotGreaterThan (leftIndex, numFrames - 1));
//        }
//
//        F getSample(int octave, int leftIndex, F framePos, F phase) const {
//            F x0 { tables[octave][leftIndex][phase] };
//            F x1 { tables[octave][jmin(leftIndex + 1, numFrames - 1)][phase] };
//            jassert(abs(x0) < F(1.5));
//            if (framePos > zero<F>)
//                jassert(abs(x1) < F(1.5));
//            return jmap (framePos, x0, x1);
//        }
//
//        template<typename AnyFloatType> void copyTableData(const PhaseTableSet<AnyFloatType>& newSet) {
//            for (int i { 0 }; i < waveLength * maxFrames * 2 + 1; ++i)
//                tableData[i] = static_cast<F>(newSet.tableData[i]);
//            numFrames = newSet.getNumFrames();
//            assignTablePointers();
//        }
//
//        static void snapToZeroCrossings(F* samples, int frameLength, int numFrames) {
//            int startIndex { 0 }, endIndex { frameLength };
//            for (int frame { 0 }; frame < numFrames; ++frame, startIndex += frameLength, endIndex += frameLength) {
//                F startDC { samples[startIndex] }, endDC { samples[endIndex] };
//                F step { (endDC - startDC) / F(frameLength) }, correction { startDC };
//                for (int i { 0 }; i < frameLength; ++i, correction += step)
//                    samples[startIndex + i] -= correction;
//            }
//        }
//
//        //MUST NOT be called from audio thread
//        void loadTables(const AudioBuffer<float>& waveTableFile) {
//            AudioBuffer<F> buffer;
//            buffer.makeCopyOf(waveTableFile);
//            numFrames = buffer.getNumSamples()/waveLength;
//            if (buffer.getNumSamples() == numFrames * waveLength) {
//                buffer.setSize(1, numFrames * waveLength + 1);
//                buffer.setSample(0, numFrames * waveLength, buffer.getSample(0, numFrames * waveLength - 1) + (buffer.getSample(0, numFrames * waveLength - 1) - buffer.getSample(0, numFrames * waveLength - 2)));
//            }
//            //            snapToZeroCrossings(buffer.getWritePointer(0), waveLength, numFrames);
//            //            buffer.applyGain(one<F>/buffer.getMagnitude(0, buffer.getNumSamples()));
//            //            buffer.applyGain(sqrt2<F>/two<F>);
//            //            buffer.applyGain(one<F>/Wave<F>::getRMS(buffer.getReadPointer(0), waveLength * numFrames));
//            FIR::Filter<F> filter;
//            *filter.coefficients = *FilterDesign<F>::designFIRLowpassHalfBandEquirippleMethod(0.1f, -50.f);
//            int filterOrder = int(filter.coefficients->getFilterOrder());
//
//            for (int i { 0 }; i < numFrames * waveLength; ++i)
//                tableData[i] = buffer.getSample(0, i);
//
//            if (numFrames == 1) {
//                for (int i { 0 }; i < waveLength; ++i)
//                    tableData[waveLength + i] = buffer.getSample(0, i);
//                numFrames = 2;
//            }
//
//            std::vector<F> temp(size_t(numFrames * 3 * waveLength), zero<F>);
//
//            for (int j { 0 }; j < 3; ++j)
//                for (int i { 0 }; i < numFrames * waveLength; ++i)
//                    temp[(size_t) (j * numFrames * waveLength + i)] = tableData[i];
//
//            // These two loops copy the first table to the slot before it and the last table to the slot after it...
//            // Hopefully this improves the filtering because the tables will not be very different
//            //            for (int i { 0 }; i < waveLength; ++i)
//            //                temp[(size_t) (numFrames * waveLength - waveLength + i)] = temp[(size_t) (numFrames * waveLength + i)];
//            //            for (int i { 0 }; i < waveLength; ++i)
//            //                temp[(size_t) (2 * numFrames * waveLength + i)] = temp[(size_t) (2 * numFrames * waveLength - waveLength + i)];
//
//            for (int o { 1 }; o < numOctaves; ++o)
//                turn3BuffersIntoOneAtHalfLength(temp.data(), tableData + getOffset(o), numFrames * getFrameLength(o), filterOrder, filter, o);
//
//            //            for (int o { 0 }, fadeLength { 4 }, frameLength { getFrameLength(o) }; o < numOctaves; ++o, //fadeLength /= 2,
//            //                 frameLength /= 2) {
//            //                F* octavePtr { tableData + getOffset(o) };
//            //                std::cout << "\n";
//            //                print("OCTAVE: ", o);
//            //                for (int i {}; i < 4; ++i)
//            //                    print("frame", i, " - BEFORE", octavePtr[frameLength * i + 0], octavePtr[frameLength * i + 1], octavePtr[frameLength * i + 2], octavePtr[frameLength * i + 3], octavePtr[frameLength * i + 4], octavePtr[frameLength * i + 5], octavePtr[frameLength * i + 6], octavePtr[frameLength * i + 7]);
//            //                for (int f { 0 }; f < numFrames; ++f) {
//            //                    xFadeEdgesToSine(octavePtr + frameLength * f, frameLength, fadeLength);
//            //                }
//            //                for (int i {}; i < 4; ++i)
//            //                    print("frame", i, " - AFTER", octavePtr[frameLength * i + 0], octavePtr[frameLength * i + 1], octavePtr[frameLength * i + 2], octavePtr[frameLength * i + 3], octavePtr[frameLength * i + 4], octavePtr[frameLength * i + 5], octavePtr[frameLength * i + 6], octavePtr[frameLength * i + 7]);
//            //            }
//
//
//
//            assignTablePointers();
//        }
//
//        constexpr int getOffset(int octave) const {
//            int o { 0 };
//            for (int i { 0 }; i < octave; ++i)
//                o += ((numFrames * waveLength) >> i);
//            return o;
//        }
//
//        void assignTablePointers() {
//            for (int i { 0 }; i < numFrames; ++i) {
//                tables[0][i].table = &tableData[waveLength * i];
//                tables[0][i].waveLength = static_cast<F>(waveLength);
//            }
//
//            for (int o { 1 }; o < numOctaves; ++o) {
//                int frameLength { getFrameLength(o) };
//                for (int i { 0 }; i < numFrames; ++i) {
//                    tables[o][i].table = &tableData[getOffset(o) + frameLength * i];
//                    tables[o][i].waveLength = static_cast<F>(frameLength);
//                }
//            }
//        }
//
//        void turn3BuffersIntoOneAtHalfLength(F* tripleSrc, F* dest, int numOutputSamples, int filterOrder, FIR::Filter<F>& filter, int o) {
//            filter.reset();
//
//            std::fill(tripleSrc, tripleSrc + numOutputSamples * 2, zero<F>);
//            std::fill(tripleSrc + numOutputSamples * 4, tripleSrc + numOutputSamples * 6, zero<F>);
//
//            //filter the triple buffer with the halfband
//            for (int i { 0 }; i < numOutputSamples * 6; ++i)
//                tripleSrc[i] = filter.processSample(tripleSrc[i]);
//
//            //            snapToZeroCrossings(tripleSrc, getFrameLength(o - 1), numFrames * 3);
//
//            //copy a downsampled (factor 2) middle part of the triple buffer to the destination
//            for (int i { 0 }; i < numOutputSamples; ++i)
//                dest[i] = tripleSrc[numOutputSamples * 2 + filterOrder/2 + i * 2];
//
//            //            snapToZeroCrossings(dest, getFrameLength(o), numFrames);
//
//            //copy 3 copies of the new decimated signal
//            for (int j { 0 }; j < 3; ++j) {
//                for (int i { 0 }; i < numOutputSamples; ++i)
//                    tripleSrc[numOutputSamples * j + i] = dest[i];
//            }
//        }
//
//        F tableData[waveLength * maxFrames * 2 + 1]{};
//        Table<F> tables[numOctaves][maxFrames];
//        int numFrames;
//    };
//}// namespace sadistic

//auto first = GL_ZERO, second = GL_ZERO;
//
//if (wtPos < 0.1f)
//first = GL_ZERO;
//else if (wtPos < 0.2f)
//first = GL_ONE;
//else if (wtPos < 0.3f)
//first = GL_DST_COLOR;
//else if (wtPos < 0.4f)
//first = GL_ONE_MINUS_DST_COLOR;
//else if (wtPos < 0.5f)
//first = GL_SRC_ALPHA;
//else if (wtPos < 0.6f)
//first = GL_ONE_MINUS_SRC_ALPHA;
//else if (wtPos < 0.7f)
//first = GL_DST_ALPHA;
//else if (wtPos < 0.8f)
//first = GL_ONE_MINUS_DST_ALPHA;
//else
//first = GL_SRC_ALPHA_SATURATE;
//
//
//if (shaperBlend < 0.1f)
//second = GL_ZERO;
//else if (shaperBlend < 0.2f)
//second = GL_ONE;
//else if (shaperBlend < 0.3f)
//second = GL_SRC_COLOR;
//else if (shaperBlend < 0.4f)
//second = GL_ONE_MINUS_SRC_COLOR;
//else if (shaperBlend < 0.5f)
//second = GL_SRC_ALPHA;
//else if (shaperBlend < 0.6f)
//second = GL_ONE_MINUS_SRC_ALPHA;
//else if (shaperBlend < 0.7f)
//second = GL_DST_ALPHA;
//else
//second = GL_ONE_MINUS_DST_ALPHA;

//            glBlendFunc (first, second);
