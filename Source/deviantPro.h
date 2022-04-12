#pragma once
#include "PhaseTableSet.h"
#include "../../Deviant/Source/Effects.h"

namespace sadistic {

    enum { WTRESOLUTION = 32, FIXEDFILTERORDER = 128 };//, WAVELENGTH = 2048 };
    enum { dials = 0, staticPad = 1, numDisplays = 2 };
    enum { gain, numModes };
    enum { dynamicPad = numDisplays, matrix = 3, numProDisplays };
    enum { phase = numModes, numProModes };
    enum { maxProParams = 5, numProFX = 6 };
    
    static inline const Colour yellowOrange { Colour::fromFloatRGBA(1.f, 0.67f, 0.2f, 1.f) }, orange { Colour::fromFloatRGBA(1.f, 0.65f, 0.f, 1.f) }, amber { Colour::fromFloatRGBA(1.f, 0.75f, 0.f, 1.f) }, neonRed { Colour::fromFloatRGBA(1.f, 0.39f, 0.f, 1.f) }, tangerine { Colour::fromFloatRGBA(0.94f, 0.5f, 0.f, 1.f) };
    
    constexpr EffectInfo effectInfoPro[numProFX] {
        { true, 1, 0, 1.f, "Dynamic Atan", "dynamicAtan", 1 },
        { true, 1, 1, 1.f, "Dynamic Bit Crusher", "dynamicBitCrusher", 1 },
        { true, 1, 2, 1.f, "Dynamic Clipper", "dynamicClipper", 1 },
        { true, 1, 3, 1.f, "Dynamic Logistic", "dynamicLogistic", 1 },
        { true, 1, 2, 1.f, "Dynamic Hyperbolic", "dynamicHyperbolic", 1 },
        { true, 1, 3, 1.f, "Dynamic Waveshaper", "dynamicWaveShaper", 4 }
    };
    
    inline ParamInfo paramInfoPro[numProFX][maxProParams] {
        { { ParamInfo::NA, 0.f, 111.f, 0.f, "Drive", "Drive" } },
        { { ParamInfo::NA, 0.f, 111.f, 0.f, "Drive", "Drive" } },
        { { ParamInfo::NA, 0.f, 111.f, 0.f, "Drive", "Drive" } },
        { { ParamInfo::NA, 0.f, 111.f, 0.f, "Drive", "Drive" } },
        { { ParamInfo::NA, 0.f, 111.f, 0.f, "Drive", "Drive" } },
        { { ParamInfo::NA, 0.f, 99.f, 1.f, "Table ID", "TableID" }, { ParamInfo::NA, 0.f, 1.f, 0.f, "WT Position", "wtPos" }, { ParamInfo::Hz, 800.f, 8000.f, 2000.f, "Pre Cutoff", "PreCutoff" }, { ParamInfo::Hz, 1000.f, 20000.f, 10000.f, "Post Cutoff", "PostCutoff" } },
    };
    
    inline String getProFxID(int effectIndex) { return { effectInfoPro[effectIndex].id }; }
    inline String getProFxName(int effectIndex) { return { effectInfoPro[effectIndex].name }; }
    inline String getProParamID(int eIndex, int pIndex) { return { getProFxID(eIndex) + String(paramInfoPro[eIndex][pIndex].id) }; }
    inline String getProParamName(int eIndex, int pIndex) { return { getProFxName(eIndex) + " " + String(paramInfoPro[eIndex][pIndex].name) }; }
    constexpr int getNumParamsForProEffect(int effectIndex) { return effectInfoPro[effectIndex].numParams; }
    
    template<typename Cs, typename F>
//    inline F shapeSample(const Cs& aC, const Cs& bC, const Cs& cC, const Cs& dC, const Cs& hC, F sample, F blend = 1.f) {
    inline F shapeSample(const Cs&, const Cs&, const Cs&, const Cs&, const Cs&, F sample, F = 1.f) {
        return sample;//blend * Hyperbolic::processSample(Clipper::processSample(Logistic::processSample(Atan::processSample(Crusher::processSample(sample, bC), aC), dC), cC), hC) + (F(1) - blend) * sample;
    }
    
    template<typename Cs, typename F>
    inline F shapeSampleX(const Cs& aC, const Cs& bC, const Cs& cC, const Cs& dC, const Cs& hC, F sample, F blend = 1.f) {
        return blend * Hyperbolic::processSample(Clipper::processSample(Logistic::processSample(Atan::processSample(Crusher::processSample(sample, bC), aC), dC), cC), hC) + (F(1) - blend) * sample; }
    
    template<typename F, int bufferLength, int filterOrder, int latency> struct FixedSizePreProcessFilterCompensator {
        void setFilterDelay(int fD) { filterDelay.setDelay(fD); }
        void setProcessDelay(int pD) { processDelay.setDelay(pD); }
        void prepare(const ProcessSpec& spec) {
            mBuffer.setSize((int) spec.numChannels, bufferLength);
            filterDelay.prepare(spec);
            processDelay.prepare(spec);
        }
        void reset() {
            filterDelay.reset();
            processDelay.reset();
        }
        void copyBuffer(AudioBuffer<F>& buffer) {
            filterDelay.process(buffer, mBuffer);
        }
        void subtractBuffer(AudioBuffer<F>& buffer) {
            AudioBlock<F> block { buffer }, mBlock { mBuffer };
            mBlock -= block;
        }
        void addBuffer(AudioBuffer<F>& buffer) {
            if (latency > 0) processDelay.process(mBuffer);
            AudioBlock<F> block { buffer }, mBlock { mBuffer };
            block += mBlock;
        }
        AudioBuffer<F> mBuffer;
        SadDelay<F, filterOrder/2> filterDelay;
        SadDelay<F, latency> processDelay;
    };
    
    template<typename F, int bufferLength, int filterOrder, int latency> struct FixedSizePostProcessFilterCompensator {
        void setFilter(typename FIR::Coefficients<F>::Ptr s) {
            *filter.state = *s;
            auto* coeffs { s->getRawCoefficients() };
            auto* mCoeffs { filter.state->getRawCoefficients() };
            jassert(s->getFilterOrder() == filterOrder);
            for (size_t i { 0 }; i <= s->getFilterOrder(); ++i)
                mCoeffs[i] = coeffs[i];// * static_cast<F>(pow(-1, i));
        }
        void setProcessDelay(int pD) { processDelay.setDelay(pD); }
        void prepare(const ProcessSpec& spec) {
            mBuffer.setSize((int) spec.numChannels, bufferLength);
            processDelay.prepare(spec);
            filter.prepare(spec);
        }
        void reset() {
            filter.reset();
            processDelay.reset();
        }
        void copyBuffer(AudioBuffer<F>& buffer) {
            processDelay.process(buffer, mBuffer);
            AudioBlock<F> mBlock { mBuffer };
            filter.process(ProcessContextReplacing<F>(mBlock));
        }
        void addBuffer(AudioBuffer<F>& buffer) {
            AudioBlock<F> block { buffer }, mBlock { mBuffer };
            block += mBlock;
        }
        AudioBuffer<F> mBuffer;
        ProcessorDuplicator<FIR::Filter<F>, FIR::Coefficients<F>> filter;
        SadDelay<F, latency> processDelay;
    };
    
    struct TableManager {
        static constexpr int gainLength { GAINLENGTH };
        
        TableManager(std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) : coeffIdx(cI), coeffs(cS) {
            for (int i { 0 }; i <= gainLength; ++i) inputTable[i] = -1.f + 2.f * float(i) / float(gainLength); }
        
        template <typename F> void makeStaticTable(F* dest = nullptr) {
            const auto& atanCoeffs { coeffs[0][int(coeffIdx[0])] };
            const auto& crusherCoeffs { coeffs[1][int(coeffIdx[1])] };
            const auto& clipperCoeffs { coeffs[2][int(coeffIdx[2])] };
            const auto& deviationCoeffs { coeffs[3][int(coeffIdx[3])] };
            const auto& hyperbolicCoeffs { coeffs[4][int(coeffIdx[4])] };
            makeTable(atanCoeffs, crusherCoeffs, clipperCoeffs, deviationCoeffs, hyperbolicCoeffs, gainTable);
            if (dest) for (size_t i { 0 }; i <= size_t(gainLength); ++i) dest[i] = static_cast<F>(gainTable[i]);
            int extremity;
            const int indexOfMax { static_cast<int>(std::distance(gainTable,std::max_element(gainTable, gainTable + gainLength + 1))) };
            const int indexOfMin { static_cast<int>(std::distance(gainTable,std::min_element(gainTable, gainTable + gainLength + 1))) };
            if(abs(gainTable[indexOfMax]) > abs(gainTable[indexOfMin])) extremity = indexOfMax;
            else extremity = indexOfMin;
            const auto mag { abs(gainTable[extremity]) }, magDB { Decibels::gainToDecibels(mag) }, mult { 1.f - -magDB/100.f };
            waveMult = powf(mult, 7.f);
            newGUIDataHere = true;
        }
        
        template<typename COEFFS, typename F>
        void makeTable(const COEFFS& aC, const COEFFS& bC, const COEFFS& cC, const COEFFS& dC, const COEFFS& hC, F* table, float shaperBlend = 1.f) {
            const auto blend { static_cast<F>(shaperBlend) };
            for (int i { 0 }; i <= gainLength; ++i) {
                const auto gainSample { static_cast<F>(-1.f + 2.f * float(i) / float(gainLength)) };
                table[i] = blend * shapeSample(aC, bC, cC, dC, hC, gainSample) + (F(1.0) - blend) * gainSample;
            }
        }
        
        float inputTable[gainLength + 1], gainTable[gainLength + 1];
        float waveMult { 1.f };
        std::atomic<bool> newGUIDataHere { true };
        std::atomic<int>* coeffIdx;
        float(& coeffs)[numFX][maxCoeffs][maxCoeffs];
    };
    
    struct WaveTableManager {
        static constexpr int waveLength { WAVELENGTH }, gainLength { GAINLENGTH };
        
        WaveTableManager(APVTS& a, UndoManager* uM, std::atomic<int>* sI, float(& sCS)[numFX][maxCoeffs][maxCoeffs], std::atomic<int>* cI, float(& cS)[numFX][maxCoeffs][maxCoeffs]) : mgr(sI, sCS), apvts(a), undoManager(uM), coeffIdx(cI), coeffs(cS) {
            auto sadPath { getSadisticFolderPath() };
            for (const auto& file : File (sadPath + "/Wave Tables/Stock/Basic").findChildFiles (2, false, "*.wav"))
                basicTableNames.add ("/Wave Tables/Stock/Basic/" + file.getFileName());
            basicTableNames.sortNatural();
            for (const auto& file : File (sadPath + "/Wave Tables/Stock").findChildFiles (2, false, "*.wav"))
                stockTableNames.add ("/Wave Tables/Stock/" + file.getFileName());
            stockTableNames.sortNatural();
            for (const auto& file : File (sadPath + "/Wave Tables/User").findChildFiles (2, true, "*.wav")) {
                auto relativePath { file.getRelativePathFrom(File (sadPath + "/Wave Tables/User")) };
                userTableNames.add ("/Wave Tables/User/" + relativePath);
            }
            userTableNames.sortNatural();
            loadTable(File(sadPath + "/" + currentTable));
        }

        bool loadTable(File inputFile) {
            WavAudioFormat format;
            auto inputStream { FileInputStream(inputFile) };
            auto* reader { format.createReaderFor(&inputStream, true) };
            if (reader) {
                SamplerSound ss { {}, *reader, {}, 10, 10.0, 10.0, 30.0 };
                auto* bufferPtr { ss.getAudioData() };
                phaseTableSet.loadTables(*bufferPtr);
                newPhaseTableSetAvailable = true;
                guiNewPhaseTableSetAvailable = true;
                currentTable = inputFile.getRelativePathFrom(getSadisticFolderPath() + "/Wave Tables");
                return true;
            }
            return false;
        }

        const String& getCurrentTable() { return currentTable; }
        
        template <typename F> void makeDynamicTable() {
            const auto& atanCoeffs { coeffs[0][int(coeffIdx[0])] };
            const auto& crusherCoeffs { coeffs[1][int(coeffIdx[1])] };
            const auto& clipperCoeffs { coeffs[2][int(coeffIdx[2])] };
            const auto& deviationCoeffs { coeffs[3][int(coeffIdx[3])] };
            const auto& hyperbolicCoeffs { coeffs[4][int(coeffIdx[4])] };
            float blend { *apvts.getRawParameterValue("dynamicWaveShaperBlend") };
            mgr.makeTable(atanCoeffs, crusherCoeffs, clipperCoeffs, deviationCoeffs, hyperbolicCoeffs, gainTable, blend);
            int extremity;
            const int indexOfMax { static_cast<int>(std::distance(gainTable,std::max_element(gainTable, gainTable + gainLength + 1))) };
            const int indexOfMin { static_cast<int>(std::distance(gainTable,std::min_element(gainTable, gainTable + gainLength + 1))) };
            if(abs(gainTable[indexOfMax]) > abs(gainTable[indexOfMin])) extremity = indexOfMax;
            else extremity = indexOfMin;
            const auto mag { abs(gainTable[extremity]) }, magDB { Decibels::gainToDecibels(mag) }, mult { 1.f - -magDB/100.f };
            gainMult = mult;
            mgr.newGUIDataHere = true;
        }

        TableManager mgr;
        APVTS& apvts;
        UndoManager* undoManager;
        std::atomic<int>* coeffIdx;
        float(& coeffs)[numFX][maxCoeffs][maxCoeffs];
        AudioBuffer<float> currentPhaseTables;
        float gainMult { 1.f }, phaseTable[gainLength + 1]{}, gainTable[gainLength + 1]{};
        StringArray basicTableNames, stockTableNames, userTableNames;
        String currentTable { "Wave Tables/Stock/MATRIXYC64.wav" };
        PhaseTableSet<float> phaseTableSet;
        bool newPhaseTableSetAvailable { false }, guiNewPhaseTableSetAvailable { false };
    };
    
    ////////////////////////    Graphics    ///////////////////////////////
    
    
    
    struct WaveTableBuffer {
        static constexpr int scopeSize { SCOPESIZE }, wtResolution { WTRESOLUTION };
        class FrameWithState {
        public:
            void reset() { std::fill(frame, frame + sizeof(frame)/sizeof(float), 0.f); isReady = false; wasFirstIn = false; }
            const float* getReadPointer() { return frame; }
            float* getWritePointer() { reset(); return frame; }
            bool isSameAddress(const float* frameToCheck) { if(frameToCheck == frame) return true; else return false; }
            std::atomic<bool> isReady { false }, wasFirstIn { false }, wasLastOut { false };
        private:
            float frame[wtResolution * (wtResolution + 1)];
        };
        
        void reset() { frameA.reset(); frameB.reset(); }
        float* getBlankFrame() { return writeFrame->getWritePointer(); }
        void setReadyToRender(float* = nullptr) { std::swap(writeFrame, readFrame); newDataAvailableToRender = true; }
        const float* getFrameToRead() { return readFrame->getReadPointer(); }
        void finishedRendering(const float*) { newDataAvailableToRender = false; }
        void flush() { reset(); frameA.isReady = true; }

        FrameWithState frameA, frameB;
        FrameWithState* writeFrame { &frameA }, * readFrame { &frameB };
        std::atomic<bool> newDataAvailableToRender { false };
    };
    
    
    struct LeftEmpiricalLAF : EmpiricalLAF { void drawLabel (Graphics& g, Label& label) override; };
    struct RightEmpiricalLAF : EmpiricalLAF { void drawLabel (Graphics& g, Label& label) override; };
    void showInteger111Value(Slider& slider, Label& label1, Label& label2);
    
    struct SadBox : ComboBox {
        SadBox(String name, WaveTableManager& m) : ComboBox(name), mgmt(m) {
            auto* menu { getRootMenu() };
            int i { 1 };
            
            for (int j { 0 }; j < mgmt.basicTableNames.size(); ++i, ++j) {
                PopupMenu::Item item { mgmt.basicTableNames[j] };
                item.itemID = i;
                String fileName { getSadisticFolderPath() + "/" + mgmt.basicTableNames[j] };
                auto file { File(fileName) };
                item.action = { [file, this]{ mgmt.loadTable( file ); setText(file.getFileNameWithoutExtension()); } };
                menu->addItem(item);
            }
            
            for (int j { 0 }; j < mgmt.stockTableNames.size(); ++i, ++j) {
                PopupMenu::Item item { mgmt.stockTableNames[j] };
                item.itemID = i;
                String fileName { getSadisticFolderPath() + "/" + mgmt.stockTableNames[j] };
                auto file { File(fileName) };
                item.action = { [file, this]{ mgmt.loadTable( file ); setText(file.getFileNameWithoutExtension()); } };
                menu->addItem(item);
            }
            
            for (int j { 0 }; j < mgmt.userTableNames.size(); ++i, ++j) {
                PopupMenu::Item item { mgmt.userTableNames[j] };
                item.itemID = i;
                String fileName { getSadisticFolderPath() + "/" + mgmt.userTableNames[j] };
                auto file { File(fileName) };
                item.action = { [file, this]{ mgmt.loadTable( file ); setText(file.getFileNameWithoutExtension()); } };
                menu->addItem(item);
            }
            
            PopupMenu::Item itemLoad { "Load Table" };
            itemLoad.itemID = int(i++);
            itemLoad.action = { [&,this](){ loadFile(); } };
            menu->addItem(itemLoad);
            PopupMenu::Item itemSave { "Save Table" };
            itemSave.itemID = int(i++);
            itemSave.action = { [&,this](){ saveFile(); } };
            menu->addItem(itemSave);
            saveCallback = [&,this] (const FileChooser& chooser) {
                auto result { chooser.getResult() };
                if(saveTable(result)) juce::AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Table Saved...", ""); };
            saveFile = [&,this](){
                fc = std::make_unique<FileChooser>("Save Wave Table", File(), "*.wav");
                fc->launchAsync (FileBrowserComponent::saveMode |
                                 FileBrowserComponent::canSelectFiles |
                                 FileBrowserComponent::warnAboutOverwriting |
                                 FileBrowserComponent::doNotClearFileNameOnRootChange,
                                 saveCallback); };
            loadCallback = [&,this] (const FileChooser& chooser) {
                if (!chooser.getResults().isEmpty()) {
                    File file { chooser.getResult() };
                    String fileName { chooser.getResult().getFullPathName() };
                    if (mgmt.loadTable(file)) {
                        String tableName { chooser.getResult().getFileNameWithoutExtension() };
                        if(!mgmt.userTableNames.contains(tableName)) {
                            mgmt.userTableNames.add (tableName);
                            PopupMenu::Item item { tableName };
                            item.itemID = getNumItems();
                            item.action = { [=, this]{ mgmt.loadTable( file ); } };
                            menu->addItem(item);
                        }
                        juce::AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Table Loaded", "!");
                    }
                    else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not loaded correctly", "!");
                }
                else juce::AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "File not choosen correctly", "!"); };
            loadFile = [&,this](){
                fc = std::make_unique<FileChooser>("Load Wave Table (.wav)", File(), "*.wav");
                fc->launchAsync (FileBrowserComponent::openMode |
                                 FileBrowserComponent::canSelectFiles,
                                 loadCallback); };
            setText(mgmt.getCurrentTable().fromLastOccurrenceOf("/", false, true).dropLastCharacters(4));
        }
        
        bool saveTable(File&) {
//            return saveTable(outputFile, mgmt.getCurrentTable());
            return false;
        }
        
        bool static saveTable(File& outputFile, const AudioBuffer<float>& buffer) {
            StringPairArray metadataValues = WavAudioFormat::createBWAVMetadata ("Custom WaveTable", "originator", "originatorRef", Time::getCurrentTime(), buffer.getNumChannels(), "codingHistory");
            std::unique_ptr<juce::FileOutputStream> outStream;
            outStream = outputFile.createOutputStream();
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer;
            writer.reset(format.createWriterFor(outStream.get(), 44100.0, uint32(buffer.getNumChannels()), 32, metadataValues, 0));
            
            if (writer != nullptr) {
                outStream.release();
                if (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples())) return true;
            }
            return false;
        }
        
        WaveTableManager& mgmt;
        std::unique_ptr<FileChooser> fc;
        std::function<void()> loadFile, saveFile;
        std::function<void(int)> selectTable;
        std::function<void (const FileChooser&)> loadCallback, saveCallback;
    };
}

