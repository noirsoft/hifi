//
//  Shape.h
//  libraries/shared/src
//
//  Created by Andrew Meadows on 2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Shape_h
#define hifi_Shape_h

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <QtGlobal>
#include <QVector>

class PhysicsEntity;
class VerletPoint;

const float MAX_SHAPE_MASS = 1.0e18f; // something less than sqrt(FLT_MAX)

class Shape {
public:
    static quint32 getNextID() { static quint32 nextID = 0; return ++nextID; }

    enum Type{
        UNKNOWN_SHAPE = 0,
        SPHERE_SHAPE,
        CAPSULE_SHAPE,
        PLANE_SHAPE,
        LIST_SHAPE
    };

    Shape() : _type(UNKNOWN_SHAPE), _owningEntity(NULL), _boundingRadius(0.f), 
            _translation(0.f), _rotation(), _mass(MAX_SHAPE_MASS) {
        _id = getNextID();
    }
    virtual ~Shape() { }

    int getType() const { return _type; }
    quint32 getID() const { return _id; }

    void setEntity(PhysicsEntity* entity) { _owningEntity = entity; }
    PhysicsEntity* getEntity() const { return _owningEntity; }

    float getBoundingRadius() const { return _boundingRadius; }

    virtual const glm::quat& getRotation() const { return _rotation; }
    virtual void setRotation(const glm::quat& rotation) { _rotation = rotation; }

    virtual void setTranslation(const glm::vec3& translation) { _translation = translation; }
    virtual const glm::vec3& getTranslation() const { return _translation; }

    virtual void setMass(float mass) { _mass = mass; }
    virtual float getMass() const { return _mass; }

    virtual bool findRayIntersection(const glm::vec3& rayStart, const glm::vec3& rayDirection, float& distance) const = 0;

    /// \param penetration of collision
    /// \param contactPoint of collision
    /// \return the effective mass for the collision
    /// For most shapes has side effects: computes and caches the partial Lagrangian coefficients which will be
    /// used in the next accumulateDelta() call.
    virtual float computeEffectiveMass(const glm::vec3& penetration, const glm::vec3& contactPoint) { return _mass; }

    /// \param relativeMassFactor the final ingredient for partial Lagrangian coefficients from computeEffectiveMass()
    /// \param penetration the delta movement
    virtual void accumulateDelta(float relativeMassFactor, const glm::vec3& penetration) {}

    virtual void applyAccumulatedDelta() {}

    /// \return volume of shape in cubic meters
    virtual float getVolume() const { return 1.0; }

    virtual void getVerletPoints(QVector<VerletPoint*>& points) {}

protected:
    // these ctors are protected (used by derived classes only)
    Shape(Type type) : _type(type), _owningEntity(NULL), _boundingRadius(0.f), _translation(0.f), _rotation() {
        _id = getNextID();
    }

    Shape(Type type, const glm::vec3& position) 
        : _type(type), _owningEntity(NULL), _boundingRadius(0.f), _translation(position), _rotation() {
        _id = getNextID();
    }

    Shape(Type type, const glm::vec3& position, const glm::quat& rotation) 
        : _type(type), _owningEntity(NULL), _boundingRadius(0.f), _translation(position), _rotation(rotation) {
        _id = getNextID();
    }

    void setBoundingRadius(float radius) { _boundingRadius = radius; }

    int _type;
    unsigned int _id;
    PhysicsEntity* _owningEntity;
    float _boundingRadius;
    glm::vec3 _translation;
    glm::quat _rotation;
    float _mass;
};

#endif // hifi_Shape_h
