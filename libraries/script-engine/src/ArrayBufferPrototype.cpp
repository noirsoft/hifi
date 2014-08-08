//
//  ArrayBufferPrototype.cpp
//
//
//  Created by Clement on 7/3/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <glm/glm.hpp>

#include "ArrayBufferClass.h"
#include "ArrayBufferPrototype.h"

Q_DECLARE_METATYPE(QByteArray*)

ArrayBufferPrototype::ArrayBufferPrototype(QObject* parent) : QObject(parent) {
}

QByteArray ArrayBufferPrototype::slice(qint32 begin, qint32 end) const {
    QByteArray* ba = thisArrayBuffer();
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? ba->size() + begin : begin;
    end = (end < 0) ? ba->size() + end : end;
    
    // here we clamp the indices to fit the array
    begin = glm::clamp(begin, 0, (ba->size() - 1));
    end = glm::clamp(end, 0, (ba->size() - 1));
    
    return (end - begin > 0) ? ba->mid(begin, end - begin) : QByteArray();
}

QByteArray ArrayBufferPrototype::slice(qint32 begin) const {
    QByteArray* ba = thisArrayBuffer();
    // if indices < 0 then they start from the end of the array
    begin = (begin < 0) ? ba->size() + begin : begin;
    
    // here we clamp the indices to fit the array
    begin = glm::clamp(begin, 0, (ba->size() - 1));
    
    return ba->mid(begin, -1);
}

QByteArray* ArrayBufferPrototype::thisArrayBuffer() const {
    return qscriptvalue_cast<QByteArray*>(thisObject().data());
}
