#pragma once
#include "deviantPro.h"
#include "wtShaders.h"
#include "../../Deviant/Source/shaders.h"

namespace sadistic {
    
    using namespace juce::gl;
    
    class WaveTableRenderer :  public Component, public juce::OpenGLRenderer {
    public:
        
        static constexpr int scopeSize { SCOPESIZE }, wtResolution { WTRESOLUTION };
        
        WaveTableRenderer(APVTS& state, ScopeBuffer(& wf)[numSignals], WaveTableBuffer& w) : apvts(state), scopeBuffer(wf), waveTableBuffer(w) {
            mapSamplesToVertices();
            mapWaveTableSamplesToVertices();
            setUpWaveTableIndices();
            mapActiveWaveTableSamplesToVertices();
            openGLContext.setOpenGLVersionRequired (OpenGLContext::OpenGLVersion::openGL3_2);
            openGLContext.detach();
            openGLContext.setRenderer(this);
            openGLContext.attachTo(*this);
            openGLContext.setContinuousRepainting(true);
        }
        void resized() override { resizedFlag = true; }
        void resizedCallback() {
            setMatrix();
            setWaveTableMatrix();
            waveTableShader->use();
            wtUniforms["matrix"]->setMatrix4 (waveTableMatrix.mat, 1, false);
            waveTableActiveWaveShader->use();
            wActiveUniforms["matrix"]->setMatrix4 (waveTableMatrix.mat, 1, false);
            waveTableActiveMoonShader->use();
            mActiveUniforms["matrix"]->setMatrix4 (waveTableMatrix.mat, 1, false);
            moonShader->use();
            mUniforms["matrix"]->setMatrix4 (matrix.mat, 1, false);
            waveShader->use();
            wUniforms["matrix"]->setMatrix4 (matrix.mat, 1, false);
        }
        ~WaveTableRenderer() override { openGLContext.detach(); }
        void openGLContextClosing() override { moonShader->release(); waveShader->release(); moonTexture.release(); }
        
        void setMatrix(float = 1.f) {
            const auto rotation { Matrix3D<float>::rotation({ 0.f, 0.f, 0.f }) };
            const auto view { Matrix3D<float>(Vector3D<float>(0.0f, 0.0f, -10.0f)) }, viewMatrix { rotation * view };
            const auto scaleFactor { 4.f }, w { 1.f / scaleFactor }, h { w * 1.2f * getLocalBounds().toFloat().getAspectRatio (false) };
            const auto projectionMatrix { Matrix3D<float>::fromFrustum (-w, w, -h, h, 4.0f, 20.0f) };
            matrix = viewMatrix * projectionMatrix;
        }
        
        void setWaveTableMatrix(float angle = 1.f) {
            const auto rotation { Matrix3D<float>::rotation({ MathConstants<float>::halfPi/4.f/angle, MathConstants<float>::halfPi/5.f, MathConstants<float>::halfPi/7.f }) };
            const auto view { Matrix3D<float>(Vector3D<float>(0.0f, 0.0f, -25.0f)) }, viewMatrix { rotation * view };
            const auto scaleFactor { 3.f }, w { 1.f / scaleFactor }, h { w * 1.2f * getLocalBounds().toFloat().getAspectRatio (false) };
            const auto projectionMatrix { Matrix3D<float>::fromFrustum (-w, w, -h, h, 4.0f, 30.0f) };
            waveTableMatrix = viewMatrix * projectionMatrix;
        }
        
        void newOpenGLContextCreated() override {
            moonTexture.loadImage(ImageFileFormat::loadFrom (Data::moonStrip_png, Data::moonStrip_pngSize));
            addShaderToContext(openGLContext, moonShader, moonVertexShader, moonGeometryShader, moonFragmentShader, mUniforms);
            addShaderToContext(openGLContext, waveShader, waveVertexShader, waveGeometryShader, waveFragmentShader, wUniforms);
            addShaderToContext(openGLContext, waveTableActiveMoonShader, wtActiveMoonVertexShader, wtActiveMoonGeometryShader, wtActiveMoonFragmentShader, mActiveUniforms);
            addShaderToContext(openGLContext, waveTableActiveWaveShader, wtActiveWaveVertexShader, wtActiveWaveGeometryShader, wtActiveWaveFragmentShader, wActiveUniforms);
            addShaderToContext(openGLContext, waveTableShader, waveTableVertexShader, nullptr, waveTableFragmentShader, wtUniforms);
            for (GLuint signal = 0; signal < numSignals; ++signal)  openGLContext.extensions.glGenBuffers (1, &vbo[signal]);
            openGLContext.extensions.glGenBuffers (1, &wtVBO);
            openGLContext.extensions.glGenBuffers (1, &wtActiveVBO);
            openGLContext.extensions.glGenBuffers (1, &wtEBO);
            
            Colour wtColour { tangerine.darker() }, wColour { neonRed };
            
            waveTableShader->use();
            wtUniforms["colour"]->set(wtColour.getFloatRed(), wtColour.getFloatGreen(), wtColour.getFloatBlue());

            waveTableActiveWaveShader->use();
            wActiveUniforms["colour"]->set(wColour.getFloatRed(), wColour.getFloatGreen(), wColour.getFloatBlue());
            
            waveTableActiveMoonShader->use();
            mActiveUniforms["colour"]->set(wColour.getFloatRed(), wColour.getFloatGreen(), wColour.getFloatBlue());
            mActiveUniforms["demoTexture"]->set((GLint) 0);
        }
        
        void renderOpenGL() override {
            jassert (OpenGLHelpers::isContextActive());
            const float renderingScale = (float) openGLContext.getRenderingScale();
            glViewport (0, 0, roundToInt (renderingScale * getWidth()), roundToInt (renderingScale * getHeight()));
            OpenGLHelpers::clear (Colours::black);
            glEnable (GL_BLEND);
            
            openGLContext.extensions.glActiveTexture (GL_TEXTURE0);
            glEnable (GL_TEXTURE_2D);
            
            const float mainBlend { *apvts.getRawParameterValue("mainBlend") }, wtPos { *apvts.getRawParameterValue("dynamicWaveShaperwtPos") };
            
            if (waveTableBuffer.newDataAvailableToRender) getNewWaveTableData();
            if (resizedFlag) resizedCallback();
                
            openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, wtVBO);
            openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(wtVertices), wtVertices, GL_STATIC_DRAW);
            openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wtEBO);
            openGLContext.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wtIndices), wtIndices, GL_STATIC_DRAW);
            openGLContext.extensions.glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid*)nullptr);
            openGLContext.extensions.glEnableVertexAttribArray (0);
            
            glBlendFunc (GL_SRC_ALPHA, GL_ONE);
            
            waveTableShader->use();
            glDrawElements(GL_TRIANGLES, (wtResolution - 1) * (wtResolution - 1) * 6, GL_UNSIGNED_INT, (GLvoid*)nullptr);
            glDrawArrays(GL_LINES, 0, 2 * wtResolution * (wtResolution - 1));
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRROR_CLAMP_TO_BORDER_EXT);
            moonTexture.bind();
            
            glBlendFunc (GL_DST_ALPHA, GL_SRC_COLOR);
            
            getActiveWaveTableData(wtPos);
            
            openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, wtActiveVBO);
            openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(wtActiveVertices), wtActiveVertices, GL_STREAM_DRAW);
            openGLContext.extensions.glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid*)nullptr);
            openGLContext.extensions.glEnableVertexAttribArray (0);
            
            waveTableActiveWaveShader->use();
            glDrawArrays(GL_TRIANGLES, 0, 3 * (scopeSize-1));
            
            waveTableActiveMoonShader->use();
            mActiveUniforms["wtPosition"]->set(-(wtPos * 2.f - 1.f));
            glDrawArrays(GL_TRIANGLES, 0, 3 * (wtResolution - 1));
            
            for (GLuint signal = 0; signal < numSignals; ++signal) {

                getNewScopeData(signal);
                const float blend { powf(signal == drySignal ? 1.f - mainBlend : mainBlend, 0.75f) };
                openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, vbo[signal]);
                openGLContext.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof(vertices[signal]), vertices[signal], GL_STREAM_DRAW);
                openGLContext.extensions.glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (GLvoid*)nullptr);
                openGLContext.extensions.glEnableVertexAttribArray (0);

                Colour c { colour[signal] }, d { Colours::orange };
                
                glBlendFunc (GL_DST_ALPHA, GL_DST_ALPHA);
                moonShader->use();
                mUniforms["colour"]->set(d.getFloatRed(), d.getFloatGreen(), d.getFloatBlue());
                mUniforms["blend"]->set(blend);
                mUniforms["matrix"]->setMatrix4 (matrix.mat, 1, false);
                mUniforms["demoTexture"]->set((GLint) 0);
                glDrawArrays(GL_TRIANGLES, 0, 3 * (scopeSize-1));
                
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                waveShader->use();
                wUniforms["colour"]->set(c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue());
                wUniforms["blend"]->set(blend);
                wUniforms["matrix"]->setMatrix4 (matrix.mat, 1, false);
                glDrawArrays(GL_TRIANGLES, 0, 3 * (scopeSize-1));

                openGLContext.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
                openGLContext.extensions.glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            moonTexture.unbind();
        }
        
        void setUpWaveTableIndices() {
            int numRows { wtResolution - 1 };
            for (int row = 0; row < numRows; ++row) {
                for (int i = 0; i < numRows; ++i) {
                    wtIndices[row][i][0] = static_cast<GLuint>(row * wtResolution + i);
                    wtIndices[row][i][1] = static_cast<GLuint>((row + 1) * wtResolution + i + 1);
                    wtIndices[row][i][2] = static_cast<GLuint>(row * wtResolution + i + 1);
                    wtIndices[row][i][3] = static_cast<GLuint>(row * wtResolution + i);
                    wtIndices[row][i][4] = static_cast<GLuint>((row + 1) * wtResolution + i + 1);
                    wtIndices[row][i][5] = static_cast<GLuint>(row * wtResolution + i + 1);
                }
            }
        }
        
        void mapWaveTableSamplesToVertices() {
            const auto xIncrement { 2.f / static_cast<float>(wtResolution - 1) };
            float zIter { -1.f }, zIncrement { 2.f / static_cast<float>(wtResolution - 1) };
            for (int f = wtResolution - 1; f >=0 ; --f, zIter += zIncrement) {
                float posIter { -1.f };
                for (int i = 0; i < wtResolution; ++i) {
                    wtVertices[f][i].position[0] = posIter;
                    wtVertices[f][i].position[2] = zIter;
                    posIter += xIncrement;
//                    wtVertices[f * wtResolution + i * 2 + 1].position[0] = posIter;
//                    wtVertices[f * wtResolution + i * 2 + 1].position[2] = zIter;
                }
            }
        }
        
        void mapSamplesToVertices() {
            const auto xIncrement { 2.f / (scopeSize-1) };
            for (int signal = 0; signal < numSignals; ++signal) {
                float posIter { -1.f };
                for (int i = 0; i < (scopeSize-1); ++i) {
                    vertices[signal][i*3].position[0] = posIter;
                    posIter += xIncrement;
                    vertices[signal][i*3+2].position[0] = posIter;
                }
            }
        }
        
        void getNewScopeData(GLuint signal) {
            const auto frame { scopeBuffer[signal].getFrameToRead() };
            if (frame) {
                //set wave vertex data
                for (GLuint i = 0; i < (scopeSize - 1); ++i) {
                    vertices[signal][i*3].position[1] = frame[i]/2.f;
                    vertices[signal][i*3+2].position[1] = frame[(i+1)]/2.f;
                }
                //clear partial buffers
                if (frame[0] == 0.f || frame[scopeSize - 1] == 0.f) {
                    for (GLuint i = 0; i < (scopeSize - 1); ++i) {
                        vertices[signal][i*3].position[1] = 0.f;
                        vertices[signal][i*3+2].position[1] = 0.f;
                    }
                }
                scopeBuffer[signal].finishedRendering(frame);
            }
        }
        
        void mapActiveWaveTableSamplesToVertices() {
            const auto xIncrement { 2.f / (wtResolution - 1) };
            float posIter { -1.f };
            for (int i = 0; i < wtResolution - 1; ++i) {
                wtActiveVertices[i*3].position[0] = posIter;
                posIter += xIncrement;
                wtActiveVertices[i*3+2].position[0] = posIter;
            }
        }
        
        void getActiveWaveTableData(float wtPos) {
            
            int idx { roundToInt(wtPos * static_cast<float>(wtResolution - 2)) };
            float zPosition { -(wtPos * 2 - 1.f) };

            //set wave vertex data
            for (GLuint i = 0; i < wtResolution - 1; ++i) {
                wtActiveVertices[i*3].position[1] = wtVertices[idx][i].position[1];
                wtActiveVertices[i*3+2].position[1] = wtVertices[idx][i + 1].position[1];
                wtActiveVertices[i*3].position[2] = zPosition;
                wtActiveVertices[i*3+2].position[2] = zPosition;
            }
        }
        
        void getNewWaveTableData() {
            const auto* frame { waveTableBuffer.getFrameToRead() };
            if (frame) {
                //set wave vertex data
                for (GLuint f = 0; f < wtResolution; ++f) {
                    for (GLuint i = 0; i < wtResolution; ++i) {
                        wtVertices[f][i].position[1] = frame[f * wtResolution + i]/2.f;
                    }
                }
                waveTableBuffer.finishedRendering(frame);
            }
        }
        
    private:
        APVTS& apvts;
        OpenGLContext openGLContext;
        GLuint vbo[numSignals], wtVBO, wtEBO, wtActiveVBO;
        std::unique_ptr<OpenGLShaderProgram> moonShader, waveShader, waveTableShader, waveTableActiveMoonShader, waveTableActiveWaveShader;
        unique_ptr_map<String, OpenGLShaderProgram::Uniform> mUniforms { { "blend", "colour", "wtPosition", "demoTexture", "matrix" } }, wUniforms { { "blend", "colour", "wtPosition", "matrix" } }, wtUniforms { { "matrix", "colour" } }, mActiveUniforms { { "colour", "demoTexture", "wtPosition", "matrix" } }, wActiveUniforms { { "colour", "matrix" } };
        struct Vertex { float position[4] { 0.f, 0.f, 0.f, 1.f }; };
        Vertex vertices[numSignals][(scopeSize - 1) * 3];
        Vertex wtVertices[wtResolution][wtResolution];
        Vertex wtActiveVertices[(wtResolution - 1) * 3];
        GLuint wtIndices[wtResolution - 1][wtResolution - 1][6];
        OpenGLTexture moonTexture;
        const Colour colour[numSignals] { Colours::red, drySignalColour };
        ScopeBuffer(& scopeBuffer)[numSignals];
        WaveTableBuffer& waveTableBuffer;
        Matrix3D<float> matrix, waveTableMatrix;
        float lastWtPos { 0.f };
        bool resizedFlag { false };
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveTableRenderer)
    };
} // namespace sadistic

