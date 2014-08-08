//
//  AnimationCache.cpp
//  libraries/script-engine/src/
//
//  Created by Andrzej Kapolka on 4/14/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QRunnable>
#include <QThreadPool>

#include "AnimationCache.h"

static int animationPointerMetaTypeId = qRegisterMetaType<AnimationPointer>();

AnimationCache::AnimationCache(QObject* parent) :
    ResourceCache(parent) {
}

AnimationPointer AnimationCache::getAnimation(const QUrl& url) {
    if (QThread::currentThread() != thread()) {
        AnimationPointer result;
        QMetaObject::invokeMethod(this, "getAnimation", Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(AnimationPointer, result), Q_ARG(const QUrl&, url));
        return result;
    }
    return getResource(url).staticCast<Animation>();
}

QSharedPointer<Resource> AnimationCache::createResource(const QUrl& url, const QSharedPointer<Resource>& fallback,
        bool delayLoad, const void* extra) {
    return QSharedPointer<Resource>(new Animation(url), &Resource::allReferencesCleared);
}

Animation::Animation(const QUrl& url) :
    Resource(url),
    _isValid(false) {
}

class AnimationReader : public QRunnable {
public:

    AnimationReader(const QWeakPointer<Resource>& animation, QNetworkReply* reply);
    
    virtual void run();

private:
    
    QWeakPointer<Resource> _animation;
    QNetworkReply* _reply;
};

AnimationReader::AnimationReader(const QWeakPointer<Resource>& animation, QNetworkReply* reply) :
    _animation(animation),
    _reply(reply) {
}

void AnimationReader::run() {
    QSharedPointer<Resource> animation = _animation.toStrongRef();
    if (!animation.isNull()) {
        QMetaObject::invokeMethod(animation.data(), "setGeometry",
            Q_ARG(const FBXGeometry&, readFBX(_reply->readAll(), QVariantHash())));
    }
    _reply->deleteLater();
}

QStringList Animation::getJointNames() const {
    if (QThread::currentThread() != thread()) {
        QStringList result;
        QMetaObject::invokeMethod(const_cast<Animation*>(this), "getJointNames", Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QStringList, result));
        return result;
    }
    QStringList names;
    foreach (const FBXJoint& joint, _geometry.joints) {
        names.append(joint.name);
    }
    return names;
}

QVector<FBXAnimationFrame> Animation::getFrames() const {
    if (QThread::currentThread() != thread()) {
        QVector<FBXAnimationFrame> result;
        QMetaObject::invokeMethod(const_cast<Animation*>(this), "getFrames", Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(QVector<FBXAnimationFrame>, result));
        return result;
    }
    return _geometry.animationFrames;
}

void Animation::setGeometry(const FBXGeometry& geometry) {
    _geometry = geometry;
    finishedLoading(true);
    _isValid = true;
}

void Animation::downloadFinished(QNetworkReply* reply) {
    // send the reader off to the thread pool
    QThreadPool::globalInstance()->start(new AnimationReader(_self, reply));
}


AnimationDetails::AnimationDetails() :
    role(), url(), fps(0.0f), priority(0.0f), loop(false), hold(false), 
    startAutomatically(false), firstFrame(0.0f), lastFrame(0.0f), running(false), frameIndex(0.0f) 
{
}

AnimationDetails::AnimationDetails(QString role, QUrl url, float fps, float priority, bool loop,
    bool hold, bool startAutomatically, float firstFrame, float lastFrame, bool running, float frameIndex) :
    role(role), url(url), fps(fps), priority(priority), loop(loop), hold(hold), 
    startAutomatically(startAutomatically), firstFrame(firstFrame), lastFrame(lastFrame), 
    running(running), frameIndex(frameIndex) 
{ 
}


QScriptValue animationDetailsToScriptValue(QScriptEngine* engine, const AnimationDetails& details) {
    QScriptValue obj = engine->newObject();
    obj.setProperty("role", details.role);
    obj.setProperty("url", details.url.toString());
    obj.setProperty("fps", details.fps);
    obj.setProperty("priority", details.priority);
    obj.setProperty("loop", details.loop);
    obj.setProperty("hold", details.hold);
    obj.setProperty("startAutomatically", details.startAutomatically);
    obj.setProperty("firstFrame", details.firstFrame);
    obj.setProperty("lastFrame", details.lastFrame);
    obj.setProperty("running", details.running);
    obj.setProperty("frameIndex", details.frameIndex);
    return obj;
}

void animationDetailsFromScriptValue(const QScriptValue& object, AnimationDetails& details) {
    // nothing for now...
}

