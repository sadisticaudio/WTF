#pragma once
#include "MembersPro.h"
namespace sadistic {
    class WTF : public AudioProcessor {
    public:
        
        //defined in cpp
        WTF(APVTS::ParameterLayout);
        AudioProcessorEditor* createEditor() override;
        
        //AudioProcessor overrides
        bool canApplyBusCountChange (bool, bool, BusProperties&) override { return true; }
        bool supportsDoublePrecisionProcessing() const override { return true; }
        const String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const String getProgramName (int) override { return {}; }
        void changeProgramName (int, const String&) override {}
        bool hasEditor() const override { return true; }
        bool isBusesLayoutSupported (const BusesLayout& layout) const override {
            //If main buses are both either one or two channels, we are good
            if (layout.getMainInputChannelSet() == layout.getMainOutputChannelSet() &&
                layout.inputBuses.size() == 1 &&
                layout.outputBuses.size() == 1)
                return true;
            return false;
        }
        void getStateInformation (MemoryBlock& destinationBlockForAPVTS) override {
            
            apvts.state.setProperty(Identifier("waveTableFile"), var(mgmt.getCurrentTable()), &undoManager);
            
            MemoryOutputStream stream(destinationBlockForAPVTS, false);
            apvts.state.writeToStream (stream); }
        void setStateInformation (const void* dataFromHost, int size) override {
            auto treeCreatedFromData { ValueTree::readFromData (dataFromHost, static_cast<size_t>(size)) };
            if (treeCreatedFromData.isValid()) {
                const String waveTablePath { apvts.state.getProperty(Identifier("waveTableFile")).toString() };
                const String newWaveTablePath { treeCreatedFromData.getProperty(Identifier("waveTableFile")).toString() };
                if (newWaveTablePath.isNotEmpty() && waveTablePath != newWaveTablePath) {
                    mgmt.loadTable(File(getSadisticFolderPath() + "/Wave Tables/" + newWaveTablePath));
                }
                apvts.state = treeCreatedFromData;
            }
        }
        
        void releaseResources() override { usingDouble() ? release(membersD) : release(membersF); }
        void prepareToPlay (double sR, int) override { usingDouble() ? prepare(sR, membersD) : prepare(sR, membersF); }
        void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override { process(buffer, membersF, false); }
        void processBlock (AudioBuffer<double>& buffer, MidiBuffer&) override { process(buffer, membersD, false); }
        void processBlockBypassed (AudioBuffer<float>& b, MidiBuffer&) override { process(b, membersF, true); }
        void processBlockBypassed (AudioBuffer<double>& b, MidiBuffer&) override { process(b, membersD, true); }
        
        //extra function definitions/templates
        BusesProperties getDefaultBusesProperties() {
            auto buses = BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true);
            return buses; }
        bool usingDouble() const { return getProcessingPrecision() == doublePrecision; }
        APVTS& getAPVTS() { return apvts; }
        UndoManager* getUndoManager() { return &undoManager; }
        LongFifo<float>* getOscilloscopeFifo() { return oscilloscopeFifo; }
        TableManager& getTableManager() { return mgmt.mgr; }
        WaveTableManager& getWaveTableManager() { return mgmt; }
        void setCurrentScreen(int s) { apvts.state.setProperty("mainCurrentScreen", var(int(s)), &undoManager); }
        int getCurrentScreen() const { return static_cast<int>(apvts.state.getProperty("mainCurrentScreen")); }
        
        template<typename FloatType> void release(WTFMembers<FloatType>& m) { m.reset(); }
        template<typename FloatType> void prepare(double sampleRate, WTFMembers<FloatType>& m) {
            auto channels { jmin(getMainBusNumInputChannels(), 2, getMainBusNumOutputChannels()) };
            dsp::ProcessSpec spec { sampleRate, (uint32) BUFFERLENGTH, (uint32) channels };
            m.prepare(spec);
            setLatencySamples(m.getLatency());
        }
        
        template<typename FloatType> void process(AudioBuffer<FloatType>& buffer, WTFMembers<FloatType>& m, bool bypassed) {
            const int numIns { getMainBusNumInputChannels() }, numOuts { getMainBusNumOutputChannels() }, channels { jmin(numIns, numOuts, 2) }, samples { buffer.getNumSamples() };
            ScopedNoDenormals noDenormals;
            AudioBuffer<FloatType> mainBuffer { buffer.getArrayOfWritePointers(), channels, samples };
            m.processBlock(mainBuffer, oscilloscopeFifo, bypassed);
        }
        
//        sadistic::SadisticMarketplaceStatus marketplaceStatus { getPluginCodeString(JucePlugin_PluginCode).getCharPointer() };
        sadistic::SadisticMarketplaceStatus marketplaceStatus { "iAnP" };
    private:
        std::atomic<int> cIdx[numFX] { 0,0,0,0,0 }, staticIdx[numFX] { 0,0,0,0,0 };
        float coefficients[numFX][maxCoeffs][maxCoeffs]{}, staticCoefficients[numFX][maxCoeffs][maxCoeffs]{};
        WTFMembers<double> membersD;
        WTFMembers<float> membersF;
        WaveTableManager mgmt;
        UndoManager undoManager;
        APVTS apvts;
        LongFifo<float> oscilloscopeFifo[2]{};
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WTF)
    };
    
    //Declaration of function that creates the editor - defined in PluginEditor.cpp
    AudioProcessorEditor* createWTFEditor(WTF&);
}
