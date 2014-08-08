//
//  BillboardOverlay.h
//
//
//  Created by Clement on 7/1/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_BillboardOverlay_h
#define hifi_BillboardOverlay_h

#include <QScopedPointer>
#include <QUrl>

#include "Base3DOverlay.h"
#include "../../renderer/TextureCache.h"

class BillboardOverlay : public Base3DOverlay {
    Q_OBJECT
public:
    BillboardOverlay();
    
    virtual void render();
    virtual void setProperties(const QScriptValue& properties);
    void setClipFromSource(const QRect& bounds) { _fromImage = bounds; }
    
private slots:
    void replyFinished();

private:
    void setBillboardURL(const QUrl url);
    
    QUrl _url;
    QByteArray _billboard;
    QSize _size;
    QScopedPointer<Texture> _billboardTexture;
    
    QRect _fromImage; // where from in the image to sample
    
    glm::quat _rotation;
    float _scale;
    bool _isFacingAvatar;
};

#endif // hifi_BillboardOverlay_h