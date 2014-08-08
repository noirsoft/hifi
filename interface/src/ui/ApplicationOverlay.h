//
//  ApplicationOverlay.h
//  interface/src/ui/overlays
//
//  Created by Benjamin Arnold on 5/27/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ApplicationOverlay_h
#define hifi_ApplicationOverlay_h

class Overlays;
class QOpenGLFramebufferObject;

const float MAGNIFY_WIDTH = 160.0f;
const float MAGNIFY_HEIGHT = 80.0f;
const float MAGNIFY_MULT = 4.0f;

// Handles the drawing of the overlays to the screen
class ApplicationOverlay {
public:

    ApplicationOverlay();
    ~ApplicationOverlay();

    void renderOverlay(bool renderToTexture = false);
    void displayOverlayTexture();
    void displayOverlayTextureOculus(Camera& whichCamera);
    void displayOverlayTexture3DTV(Camera& whichCamera, float aspectRatio, float fov);
    void computeOculusPickRay(float x, float y, glm::vec3& direction) const;
    void getClickLocation(int &x, int &y) const;
    QPoint getPalmClickLocation(const PalmData *palm) const;
    bool calculateRayUICollisionPoint(const glm::vec3& position, const glm::vec3& direction, glm::vec3& result) const;


    // Getters
    QOpenGLFramebufferObject* getFramebufferObject();
    float getAlpha() const { return _alpha; }
  
private:
    // Interleaved vertex data
    struct TextureVertex {
        glm::vec3 position;
        glm::vec2 uv;
    };

    typedef QPair<GLuint, GLuint> VerticesIndices;

    void renderPointers();
    void renderControllerPointers();
    void renderPointersOculus(const glm::vec3& eyePos);
    void renderMagnifier(int mouseX, int mouseY, float sizeMult, bool showBorder) const;
    void renderAudioMeter();
    void renderStatsAndLogs();
    void renderTexturedHemisphere();

    QOpenGLFramebufferObject* _framebufferObject;
    float _trailingAudioLoudness;
    float _textureFov;
    
    enum MagnifyDevices { MOUSE, LEFT_CONTROLLER, RIGHT_CONTROLLER, NUMBER_OF_MAGNIFIERS = RIGHT_CONTROLLER + 1 };
    bool _reticleActive[NUMBER_OF_MAGNIFIERS];
    int _mouseX[NUMBER_OF_MAGNIFIERS];
    int _mouseY[NUMBER_OF_MAGNIFIERS];
    bool _magActive[NUMBER_OF_MAGNIFIERS];
    int _magX[NUMBER_OF_MAGNIFIERS];
    int _magY[NUMBER_OF_MAGNIFIERS];
    float _magSizeMult[NUMBER_OF_MAGNIFIERS];
    
    float _alpha;
    float _oculusuiRadius;

    GLuint _crosshairTexture;
};

#endif // hifi_ApplicationOverlay_h