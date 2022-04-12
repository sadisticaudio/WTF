#pragma once
#include "../../Source/sadistic.h"

namespace sadistic {
    
    enum { WAVELENGTH = 2048 };

    template<typename F> struct Table {
        F operator[](int idx) const { return table[idx]; }
        F operator[](F sample) const {
            F floatIndex { jlimit(zero<F>, one<F>, slope * sample + intercept) * waveLength };
            auto i { truncatePositiveToUnsignedInt (floatIndex) };
            F f { floatIndex - F (i) };
            F x0 { table[i] }, x1 { table[i + 1] };
            return jmap (f, x0, x1); }
        F* table { nullptr };
        F waveLength { WAVELENGTH }, slope { one<F> }, intercept { zero<F> };
    };
    
    template<typename F> struct PhaseTableSet {
        static constexpr int waveLength { WAVELENGTH }, numOctaves { NUMOCTAVES }, maxFrames { 256 }, maxLength { maxFrames * waveLength };
        
        int getNumFrames() const { return numFrames; }
        int getFrameLength(int octave) const { return waveLength >> octave; }
        int getOctave(int len) const { int i = waveLength, o = 0; for (; i > len; i /= 2, ++o); return jlimit(0, numOctaves - 1, o); }
        void getSetPosition(const float wtPos, int& leftIndex, F& framePos) const {
            F floatIndex { jlimit(0.f, 1.f, wtPos) * float(numFrames - 1) };
            leftIndex = jlimit(0, numFrames - 1, (int)truncatePositiveToUnsignedInt (floatIndex));
            framePos = floatIndex - F (leftIndex);
            jassert (isPositiveAndNotGreaterThan (leftIndex, numFrames - 1));
        }
        
        F getSample(int octave, int leftIndex, F framePos, F phase) const {
            F x0 { tables[octave][leftIndex][phase] };
            F x1 { tables[octave][jmin(leftIndex + 1, numFrames - 1)][phase] };
            jassert(abs(x0) < F(1.5));
            jassert(abs(x1) < F(1.5));
            return jmap (framePos, x0, x1);
        }

        template<typename AnyFloatType> void copyTableData(const PhaseTableSet<AnyFloatType>& newSet) {
            for (int i { 0 }; i < waveLength * maxFrames * 2 + 1; ++i)
                tableData[i] = static_cast<F>(newSet.tableData[i]);
            numFrames = newSet.getNumFrames();
            assignTablePointers();
        }
        
        static void snapToZeroCrossings(F* samples, int frameLength, int numFrames) {
            int startIndex { 0 }, endIndex { frameLength };
            for (int frame { 0 }; frame < numFrames; ++frame, startIndex += frameLength, endIndex += frameLength) {
                F startDC { samples[startIndex] }, endDC { samples[endIndex] };
                F step { (endDC - startDC) / F(frameLength) }, correction { startDC };
                for (int i { 0 }; i < frameLength; ++i, correction += step)
                    samples[startIndex + i] -= correction;
            }
        }

        //MUST NOT be called from audio thread
        void loadTables(const AudioBuffer<float>& waveTableFile) {
            AudioBuffer<F> buffer;
            buffer.makeCopyOf(waveTableFile);
            numFrames = buffer.getNumSamples()/waveLength;
            if (buffer.getNumSamples() == numFrames * waveLength) {
                buffer.setSize(1, numFrames * waveLength + 1);
                buffer.setSample(0, numFrames * waveLength, buffer.getSample(0, numFrames * waveLength - 1) + (buffer.getSample(0, numFrames * waveLength - 1) - buffer.getSample(0, numFrames * waveLength - 2)));
            }

            FIR::Filter<F> filter;
            *filter.coefficients = *FilterDesign<F>::designFIRLowpassHalfBandEquirippleMethod(0.1f, -50.f);
            int filterOrder = int(filter.coefficients->getFilterOrder());
            
            for (int i { 0 }; i < numFrames * waveLength + 1; ++i)
                tableData[i] = buffer.getSample(0, i);
            
            if (numFrames == 1) {
                for (int i { 0 }; i < waveLength + 1; ++i)
                    tableData[waveLength + i] = buffer.getSample(0, i);
                numFrames = 2;
            }
            
//            for (int f { 0 }; f < numFrames; ++f) {
//                snapToZeroCrossings(tableData, waveLength, numFrames);
//            }

            std::vector<F> temp(size_t(numFrames * 3 * waveLength), zero<F>);
            
            for (int j { 0 }; j < 3; ++j)
                for (int i { 0 }; i < numFrames * waveLength; ++i)
                    temp[(size_t) (j * numFrames * waveLength + i)] = tableData[i];
            
            for (int o { 1 }; o < numOctaves; ++o)
                turn3BuffersIntoOneAtHalfLengthRepeatMethod(temp.data(), tableData + getOffset(o), numFrames * getFrameLength(o), filterOrder, filter, o);

            assignTablePointers();
        }
        
        constexpr int getOffset(int octave) const {
            int o { 0 };
            for (int i { 0 }; i < octave; ++i)
                o += ((numFrames * waveLength) >> i);
            return o;
        }
        
        void assignTablePointers() {
            for (int i { 0 }; i < numFrames; ++i) {
                tables[0][i].table = &tableData[waveLength * i];
                tables[0][i].waveLength = static_cast<F>(waveLength);
            }
            
            for (int o { 1 }; o < numOctaves; ++o) {
                int frameLength { getFrameLength(o) };
                for (int i { 0 }; i < numFrames; ++i) {
                    tables[o][i].table = &tableData[getOffset(o) + frameLength * i];
                    tables[o][i].waveLength = static_cast<F>(frameLength);
                }
            }
        }
        
        void turn3BuffersIntoOneAtHalfLengthContiguousMethod(F* tripleSrc, F* dest, int numOutputSamples, int filterOrder, FIR::Filter<F>& filter, int) {
            filter.reset();
            
            std::fill(tripleSrc, tripleSrc + numOutputSamples * 2, zero<F>);
            std::fill(tripleSrc + numOutputSamples * 4, tripleSrc + numOutputSamples * 6, zero<F>);

            //filter the triple buffer with the halfband
            for (int i { 0 }; i < numOutputSamples * 6; ++i)
                tripleSrc[i] = filter.processSample(tripleSrc[i]);

            //copy a downsampled (factor 2) middle part of the triple buffer to the destination
            for (int i { 0 }; i < numOutputSamples; ++i)
                dest[i] = tripleSrc[numOutputSamples * 2 + filterOrder/2 + i * 2];

            //copy 3 copies of the new decimated signal
            for (int j { 0 }; j < 3; ++j) {
                for (int i { 0 }; i < numOutputSamples; ++i)
                    tripleSrc[numOutputSamples * j + i] = dest[i];
            }
        }
        
        void turn3BuffersIntoOneAtHalfLengthZeroMethod(F* tripleSrc, F* dest, int numOutputSamples, int filterOrder, FIR::Filter<F>& filter, int o) {
            
            int frameLength { getFrameLength(o - 1) }, newFrameLength { getFrameLength(o) };
            
            F arr[waveLength * 3];
            
            for (int f { 0 }; f < numFrames; ++f) {
                filter.reset();
                std::fill(arr, arr + waveLength * 3, zero<F>);
                std::copy(tripleSrc + f * frameLength, tripleSrc + f * frameLength + frameLength, &arr[waveLength]);
                
                //filter the triple buffer with the halfband
                for (int i { 0 }; i < waveLength * 3; ++i)
                    arr[i] = filter.processSample(arr[i]);
                
                //copy a downsampled (factor 2) middle part of the triple buffer to the destination frame
                for (int i { 0 }; i < newFrameLength; ++i)
                    dest[f * newFrameLength + i] = arr[waveLength + filterOrder/2 + i * 2];
            }
            
            //copy 3 copies of the new decimated signal
            for (int j { 0 }; j < 3; ++j) {
                for (int i { 0 }; i < numOutputSamples; ++i)
                    tripleSrc[numOutputSamples * j + i] = dest[i];
            }
        }

        void turn3BuffersIntoOneAtHalfLengthRepeatMethod(F* tripleSrc, F* dest, int numOutputSamples, int filterOrder, FIR::Filter<F>& filter, int o) {
            
            int frameLength { getFrameLength(o - 1) }, newFrameLength { getFrameLength(o) };
            
            F arr[waveLength * 3];
            
            for (int f { 0 }; f < numFrames; ++f) {
                filter.reset();
                std::fill(arr, arr + waveLength * 3, zero<F>);
                std::copy(tripleSrc + f * frameLength, tripleSrc + f * frameLength + frameLength, &arr[waveLength - frameLength]);
                std::copy(tripleSrc + f * frameLength, tripleSrc + f * frameLength + frameLength, &arr[waveLength]);
                std::copy(tripleSrc + f * frameLength, tripleSrc + f * frameLength + frameLength, &arr[waveLength + frameLength]);
                
                //filter the triple buffer with the halfband
                for (int i { 0 }; i < waveLength * 3; ++i)
                    arr[i] = filter.processSample(arr[i]);
                
                //copy a downsampled (factor 2) middle part of the triple buffer to the destination frame
                for (int i { 0 }; i < newFrameLength; ++i)
                    dest[f * newFrameLength + i] = arr[waveLength + filterOrder/2 + i * 2];
            }
            
            //copy 3 copies of the new decimated signal
            for (int j { 0 }; j < 3; ++j) {
                for (int i { 0 }; i < numOutputSamples; ++i)
                    tripleSrc[numOutputSamples * j + i] = dest[i];
            }
        }
        
        F tableData[waveLength * maxFrames * 2 + 1]{};
        Table<F> tables[numOctaves][maxFrames];
        int numFrames;
    };
}// namespace sadistic
