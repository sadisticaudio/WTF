#pragma once
#include "WaveShaper.h"

namespace sadistic {
    
    template <typename F>
    struct WTFMembers {

        static constexpr int totalFX { numFX + numProFX }, numFX2Process { totalFX - numFX }, bufferLength { BUFFERLENGTH }, maxWaveOrder { 10 }, maxWavelength { 1 << maxWaveOrder }, fifoLength { maxWavelength + bufferLength }, waveLength { WAVELENGTH }, gainLength { GAINLENGTH };
        
        //Constructor for FX: emplaces references of their associated parameters into a vector member variable
        //while adding them to the APVTS, idea stolen from DSPModulePluginDemo. Thank you Reuk, Ed, Tom, and Ivan!!!
        WTFMembers(APVTS::ParameterLayout& layout, WaveTableManager& s, std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) : staticMembers(layout),
        atan(createProEffect<Shaper<Atan, F>>(layout, 0, cI[0], cS[0])),
        crusher(createProEffect<Shaper<Crusher, F>>(layout, 1, cI[1], cS[1])),
        clipper(createProEffect<Shaper<Clipper, F>>(layout, 2, cI[2], cS[2])),
        deviation(createProEffect<Shaper<Logistic, F>>(layout, 3, cI[3], cS[3])),
        hyperbolic(createProEffect<Shaper<Hyperbolic, F>>(layout, 4, cI[4], cS[4])),
        waveShaper(createProEffect<DynamicWaveShaper<F>>(layout, 5, &cI[0], cS, s)),
        params(staticMembers.params), mgmt(s) {}
        
        // like a copy constructor, made to ensure that both double and float WTFMember structs receive valid parameter references because hosts will call various double/float methods (prepareToPlay, etc...) during a session
        template<typename OtherFloatType> WTFMembers(WTFMembers<OtherFloatType>& o) : staticMembers(o.staticMembers), atan(o.atan), crusher(o.crusher), clipper(o.clipper), deviation(o.deviation), hyperbolic(o.hyperbolic), waveShaper(o.waveShaper), params(o.params), mgmt(o.mgmt) {}
        
        template <typename Effect, typename ...Ts>
        static Effect createProEffect (APVTS::ParameterLayout& layout, int effectIndex, Ts&&... ts) {
            ParamList refs;
            addParameter<AudioParameterBool>(layout, refs, getProFxID(effectIndex) + "Enabled", getProFxName(effectIndex) + " Enabled", effectInfoPro[effectIndex].defaultEnabled);
            addParameter<AudioParameterInt>(layout, refs, getProFxID(effectIndex) + "Route", getProFxName(effectIndex) + " Route", 0, 99, effectInfoPro[effectIndex].defaultRoute);
            addParameter<AudioParameterInt>(layout, refs, getProFxID(effectIndex) + "Index", getProFxName(effectIndex) + " Index", 0, 99, effectInfoPro[effectIndex].defaultIndex);
            addParameter<AudioParameterFloat>(layout, refs, getProFxID(effectIndex) + "Blend", getProFxName(effectIndex) + " Blend", 0.f, 1.f, effectInfoPro[effectIndex].defaultBlend);
            FloatParamList floatRefs;
            const auto numParamsForProEffect { getNumParamsForProEffect(effectIndex) };
            for(int i { 0 }; i < numParamsForProEffect; ++i) {
                ParamInfo info = paramInfoPro[effectIndex][i];
                addParameter<AudioParameterFloat>(layout, floatRefs, getProParamID(effectIndex, i), getProParamName(effectIndex, i), NormalisableRange<float>(info.min, info.max), info.defaultValue, getSuffix(info.type));
            }
            return Effect(getProFxID(effectIndex), refs, floatRefs, effectIndex, std::forward<Ts>(ts)...);
        }
        
        int getProLatency() { return //preFilter.getLatency() + postFilter.getLatency() +
            waveShaper.getLatency(); }
        int getLatency() { return //staticMembers.getLatency() +
            getProLatency() + bufferLength; }

        void init() {
            staticMembers.init();
            for (auto* fx : effects) fx->init();
            mgmt.mgr.makeStaticTable<F>();
            mgmt.makeDynamicTable<F>();
            waveShaper.phaseTableSet.copyTableData(mgmt.phaseTableSet);
        }
        
        void reset() {
            staticMembers.reset();
            for (auto* fx : effects) fx->reset();
        }
        
        void prepare(const ProcessSpec& spec) {
            staticMembers.prepare(spec);
            staticMembers.blendDelay.setDelay(getProLatency());
            for (auto* fx : effects) fx->prepare(spec);
            waveShaper.prepare(spec);
            mBlend = static_cast<F>(static_cast<AudioParameterFloat&>(staticMembers.params[0].get()).get());
            reset();
        }
        
        void processBlock(AudioBuffer<F>& buffer, LongFifo<float> (& oscilloscope)[2], bool bypassed = false) {
            int bufferIndex { 0 }, numSamples { buffer.getNumSamples() };
            
            while (bufferIndex < numSamples) {
                int samples { jmin(bufferLength - staticMembers.fifoIndex, numSamples - bufferIndex) };
                
                for (int j { 0 }; j < buffer.getNumChannels(); ++j) {
                    for (int i { bufferIndex }, write { staticMembers.fifoIndex }; i < bufferIndex + samples; ++i, ++write)
                        staticMembers.writeFifo[j][write] = static_cast<F>(buffer.getSample(j,i));
                    for (int i { bufferIndex }, read { staticMembers.fifoIndex }; i < bufferIndex + samples; ++i, ++read)
                        buffer.setSample(j, i, static_cast<F>(staticMembers.readFifo[j][read]));
                }
                
                staticMembers.fifoIndex += samples;
                bufferIndex += samples;
                
                if (staticMembers.fifoIndex == bufferLength) {
                    AudioBuffer<F> buf { staticMembers.writeFifo, buffer.getNumChannels(), bufferLength };
                    
//                    bool staticChanging { staticMembers.preProcess(buf) }, dynamicChanging { paramsAreChanging() };
                    bool staticChanging { false }, dynamicChanging { paramsAreChanging() };

                    process(buf, oscilloscope, bypassed, staticChanging, dynamicChanging);
                    staticMembers.fifoIndex = 0;
                    std::swap(staticMembers.writeFifo, staticMembers.readFifo);
                }
            }
        }
        
        bool paramsAreChanging() {
            bool changing { false };
            for (int i { 0 }; i < numProFX; ++i) {
                fxNeedsUpdate[i] = effects[i]->parametersNeedCooking();
                if (fxNeedsUpdate[i])
                    changing = true;
            }
            return changing;
        }

        void process(AudioBuffer<F>& buffer, LongFifo<float> (& oscilloscope)[2], bool bypassed, bool, bool) {
            AudioBlock<F> block { buffer }, blendBlock { staticMembers.blendBuffer };
            mBlend = jlimit(mBlend - F(0.01), mBlend + F(0.01), static_cast<F>(static_cast<AudioParameterFloat&>(staticMembers.params[0].get()).get()));
            
//            if (changing) mgmt.mgr.makeStaticTable<F>();
//
//            if (dynamicChanging) {
//                for (int i { 0 }; i < numProFX; ++i) {
//                    if (fxNeedsUpdate[i])
//                        effects[i]->cookParameters();
//                }
//                mgmt.makeDynamicTable<F>();
//            }
            
            staticMembers.blendDelay.process(buffer, staticMembers.blendBuffer);

            if (!bypassed) {
                waveShaper.process(buffer);
//                staticMembers.process(buffer, oscilloscope, bypassed, changing);
                oscilloscope[wetSignal].pushChannel(buffer);
                block *= mBlend;
                blendBlock *= (one<F> - mBlend);
                block += blendBlock;
            }
            else block = blendBlock;
            
            oscilloscope[drySignal].pushChannel(staticMembers.blendBuffer);
        }

        //Data Members
        DeviantMembers<F> staticMembers;

        //FX Processors
        Shaper<Atan, F> atan;
        Shaper<Crusher, F> crusher;
        Shaper<Clipper, F> clipper;
        Shaper<Logistic, F> deviation;
        Shaper<Hyperbolic, F> hyperbolic;
        DynamicWaveShaper<F> waveShaper;

        // Array of base class pointers to the above effects, similar to JUCE's ProcessorBase class,
        // with prepare, reset, and process methods
        DeviantEffect* effects[numProFX] { &atan, &crusher, &clipper, &deviation, &hyperbolic, &waveShaper };
        
        // std::vector<reference_wrapper<AudioParameterFloat>> to store parameter references that
        // are not associated with any particular effect
        ParamList params;
        
        bool fxNeedsUpdate[numProFX]{};
        
        WaveTableManager& mgmt;
        F mBlend { one<F> };
    };
}
