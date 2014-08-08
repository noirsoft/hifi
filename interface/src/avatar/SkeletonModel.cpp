//
//  SkeletonModel.cpp
//  interface/src/avatar
//
//  Created by Andrzej Kapolka on 10/17/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <glm/gtx/transform.hpp>
#include <QMultiMap>

#include <VerletCapsuleShape.h>
#include <VerletSphereShape.h>
#include <DistanceConstraint.h>
#include <FixedConstraint.h>

#include "Application.h"
#include "Avatar.h"
#include "Hand.h"
#include "Menu.h"
#include "MuscleConstraint.h"
#include "SkeletonModel.h"

SkeletonModel::SkeletonModel(Avatar* owningAvatar, QObject* parent) : 
    Model(parent),
    Ragdoll(),
    _owningAvatar(owningAvatar),
    _boundingShape(),
    _boundingShapeLocalOffset(0.0f) {
}

void SkeletonModel::setJointStates(QVector<JointState> states) {
    Model::setJointStates(states);

    // the SkeletonModel override of updateJointState() will clear the translation part
    // of its root joint and we need that done before we try to build shapes hence we
    // recompute all joint transforms at this time.
    for (int i = 0; i < _jointStates.size(); i++) {
        updateJointState(i);
    }

    clearShapes();
    if (_enableShapes) {
        buildShapes();
    }
}

const float PALM_PRIORITY = 3.0f;

void SkeletonModel::simulate(float deltaTime, bool fullUpdate) {
    setTranslation(_owningAvatar->getPosition());
    static const glm::quat refOrientation = glm::angleAxis(PI, glm::vec3(0.0f, 1.0f, 0.0f));
    setRotation(_owningAvatar->getOrientation() * refOrientation);
    const float MODEL_SCALE = 0.0006f;
    setScale(glm::vec3(1.0f, 1.0f, 1.0f) * _owningAvatar->getScale() * MODEL_SCALE);
    
    Model::simulate(deltaTime, fullUpdate);
    
    if (!(isActive() && _owningAvatar->isMyAvatar())) {
        return; // only simulate for own avatar
    }

    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    PrioVR* prioVR = Application::getInstance()->getPrioVR();
    if (prioVR->isActive()) {
        for (int i = 0; i < prioVR->getJointRotations().size(); i++) {
            int humanIKJointIndex = prioVR->getHumanIKJointIndices().at(i);
            if (humanIKJointIndex == -1) {
                continue;
            }
            int jointIndex = geometry.humanIKJointIndices.at(humanIKJointIndex);
            if (jointIndex != -1) {
                JointState& state = _jointStates[jointIndex];
                state.setRotationInBindFrame(prioVR->getJointRotations().at(i), PALM_PRIORITY);
            }
        }
        return;
    }

    // find the left and rightmost active palms
    int leftPalmIndex, rightPalmIndex;
    Hand* hand = _owningAvatar->getHand();
    hand->getLeftRightPalmIndices(leftPalmIndex, rightPalmIndex);

    const float HAND_RESTORATION_RATE = 0.25f;    
    if (leftPalmIndex == -1 || rightPalmIndex == -1) {
        // palms are not yet set, use mouse
        if (_owningAvatar->getHandState() == HAND_STATE_NULL) {
            restoreRightHandPosition(HAND_RESTORATION_RATE, PALM_PRIORITY);
        } else {
            // transform into model-frame
            glm::vec3 handPosition = glm::inverse(_rotation) * (_owningAvatar->getHandPosition() - _translation);
            applyHandPosition(geometry.rightHandJointIndex, handPosition);
        }
        restoreLeftHandPosition(HAND_RESTORATION_RATE, PALM_PRIORITY);

    } else if (leftPalmIndex == rightPalmIndex) {
        // right hand only
        applyPalmData(geometry.rightHandJointIndex, hand->getPalms()[leftPalmIndex]);
        restoreLeftHandPosition(HAND_RESTORATION_RATE, PALM_PRIORITY);

    } else {
        applyPalmData(geometry.leftHandJointIndex, hand->getPalms()[leftPalmIndex]);
        applyPalmData(geometry.rightHandJointIndex, hand->getPalms()[rightPalmIndex]);
    }

    _boundingShape.setTranslation(_translation + _rotation * _boundingShapeLocalOffset);
    _boundingShape.setRotation(_rotation);
}

void SkeletonModel::getHandShapes(int jointIndex, QVector<const Shape*>& shapes) const {
    if (jointIndex < 0 || jointIndex >= int(_shapes.size())) {
        return;
    }
    if (jointIndex == getLeftHandJointIndex()
        || jointIndex == getRightHandJointIndex()) {
        // get all shapes that have this hand as an ancestor in the skeleton heirarchy
        const FBXGeometry& geometry = _geometry->getFBXGeometry();
        for (int i = 0; i < _jointStates.size(); i++) {
            const FBXJoint& joint = geometry.joints[i];
            int parentIndex = joint.parentIndex;
            Shape* shape = _shapes[i];
            if (i == jointIndex) {
                // this shape is the hand
                if (shape) {
                    shapes.push_back(shape);
                }
                if (parentIndex != -1 && _shapes[parentIndex]) {
                    // also add the forearm
                    shapes.push_back(_shapes[parentIndex]);
                }
            } else if (shape) {
                while (parentIndex != -1) {
                    if (parentIndex == jointIndex) {
                        // this shape is a child of the hand
                        shapes.push_back(shape);
                        break;
                    }
                    parentIndex = geometry.joints[parentIndex].parentIndex;
                }
            }
        }
    }
}

void SkeletonModel::getBodyShapes(QVector<const Shape*>& shapes) const {
    // for now we push a single bounding shape, 
    // but later we could push a subset of joint shapes
    shapes.push_back(&_boundingShape);
}

void SkeletonModel::renderIKConstraints() {
    renderJointConstraints(getRightHandJointIndex());
    renderJointConstraints(getLeftHandJointIndex());
    //if (isActive() && _owningAvatar->isMyAvatar()) {
    //    renderRagdoll();
    //}
}

class IndexValue {
public:
    int index;
    float value;
};

bool operator<(const IndexValue& firstIndex, const IndexValue& secondIndex) {
    return firstIndex.value < secondIndex.value;
}

void SkeletonModel::applyHandPosition(int jointIndex, const glm::vec3& position) {
    if (jointIndex == -1 || jointIndex >= _jointStates.size()) {
        return;
    }
    // NOTE: 'position' is in model-frame
    setJointPosition(jointIndex, position, glm::quat(), false, -1, false, glm::vec3(0.0f, -1.0f, 0.0f), PALM_PRIORITY);

    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    glm::vec3 handPosition, elbowPosition;
    getJointPosition(jointIndex, handPosition);
    getJointPosition(geometry.joints.at(jointIndex).parentIndex, elbowPosition);
    glm::vec3 forearmVector = handPosition - elbowPosition;
    float forearmLength = glm::length(forearmVector);
    if (forearmLength < EPSILON) {
        return;
    }
    JointState& state = _jointStates[jointIndex];
    glm::quat handRotation = state.getRotation();

    // align hand with forearm
    float sign = (jointIndex == geometry.rightHandJointIndex) ? 1.0f : -1.0f;
    state.applyRotationDelta(rotationBetween(handRotation * glm::vec3(-sign, 0.0f, 0.0f), forearmVector), true, PALM_PRIORITY);
}

void SkeletonModel::applyPalmData(int jointIndex, PalmData& palm) {
    if (jointIndex == -1 || jointIndex >= _jointStates.size()) {
        return;
    }
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    float sign = (jointIndex == geometry.rightHandJointIndex) ? 1.0f : -1.0f;
    int parentJointIndex = geometry.joints.at(jointIndex).parentIndex;
    if (parentJointIndex == -1) {
        return;
    }
  
    // rotate palm to align with its normal (normal points out of hand's palm)
    glm::quat inverseRotation = glm::inverse(_rotation);
    glm::vec3 palmPosition = inverseRotation * (palm.getPosition() - _translation);
    glm::vec3 palmNormal = inverseRotation * palm.getNormal();
    glm::vec3 fingerDirection = inverseRotation * palm.getFingerDirection();

    glm::quat palmRotation = rotationBetween(geometry.palmDirection, palmNormal);
    palmRotation = rotationBetween(palmRotation * glm::vec3(-sign, 0.0f, 0.0f), fingerDirection) * palmRotation;

    if (Menu::getInstance()->isOptionChecked(MenuOption::AlternateIK)) {
        setHandPosition(jointIndex, palmPosition, palmRotation);  
    } else if (Menu::getInstance()->isOptionChecked(MenuOption::AlignForearmsWithWrists)) {
        float forearmLength = geometry.joints.at(jointIndex).distanceToParent * extractUniformScale(_scale);
        glm::vec3 forearm = palmRotation * glm::vec3(sign * forearmLength, 0.0f, 0.0f);
        setJointPosition(parentJointIndex, palmPosition + forearm,
            glm::quat(), false, -1, false, glm::vec3(0.0f, -1.0f, 0.0f), PALM_PRIORITY);
        JointState& parentState = _jointStates[parentJointIndex];
        parentState.setRotationInBindFrame(palmRotation, PALM_PRIORITY);
        // lock hand to forearm by slamming its rotation (in parent-frame) to identity
        _jointStates[jointIndex].setRotationInConstrainedFrame(glm::quat());
    } else {
        inverseKinematics(jointIndex, palmPosition, palmRotation, PALM_PRIORITY);
    }
}

void SkeletonModel::updateJointState(int index) {
    JointState& state = _jointStates[index];
    const FBXJoint& joint = state.getFBXJoint();
    if (joint.parentIndex != -1) {
        const JointState& parentState = _jointStates.at(joint.parentIndex);
        const FBXGeometry& geometry = _geometry->getFBXGeometry();
        if (index == geometry.leanJointIndex) {
            maybeUpdateLeanRotation(parentState, joint, state);
        
        } else if (index == geometry.neckJointIndex) {
            maybeUpdateNeckRotation(parentState, joint, state);    
                
        } else if (index == geometry.leftEyeJointIndex || index == geometry.rightEyeJointIndex) {
            maybeUpdateEyeRotation(parentState, joint, state);
        }
    }

    Model::updateJointState(index);

    if (index == _geometry->getFBXGeometry().rootJointIndex) {
        state.clearTransformTranslation();
    }
}

void SkeletonModel::maybeUpdateLeanRotation(const JointState& parentState, const FBXJoint& joint, JointState& state) {
    if (!_owningAvatar->isMyAvatar() || Application::getInstance()->getPrioVR()->isActive()) {
        return;
    }
    // get the rotation axes in joint space and use them to adjust the rotation
    glm::mat3 axes = glm::mat3_cast(glm::quat());
    glm::mat3 inverse = glm::mat3(glm::inverse(parentState.getTransform() * glm::translate(state.getDefaultTranslationInConstrainedFrame()) *
        joint.preTransform * glm::mat4_cast(joint.preRotation * joint.rotation)));
    state.setRotationInConstrainedFrame(glm::angleAxis(- RADIANS_PER_DEGREE * _owningAvatar->getHead()->getFinalLeanSideways(), 
        glm::normalize(inverse * axes[2])) * glm::angleAxis(- RADIANS_PER_DEGREE * _owningAvatar->getHead()->getFinalLeanForward(), 
        glm::normalize(inverse * axes[0])) * joint.rotation);
}

void SkeletonModel::maybeUpdateNeckRotation(const JointState& parentState, const FBXJoint& joint, JointState& state) {
    _owningAvatar->getHead()->getFaceModel().maybeUpdateNeckRotation(parentState, joint, state);
}

void SkeletonModel::maybeUpdateEyeRotation(const JointState& parentState, const FBXJoint& joint, JointState& state) {
    _owningAvatar->getHead()->getFaceModel().maybeUpdateEyeRotation(parentState, joint, state);
}

void SkeletonModel::renderJointConstraints(int jointIndex) {
    if (jointIndex == -1 || jointIndex >= _jointStates.size()) {
        return;
    }
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    const float BASE_DIRECTION_SIZE = 300.0f;
    float directionSize = BASE_DIRECTION_SIZE * extractUniformScale(_scale);
    glLineWidth(3.0f);
    do {
        const FBXJoint& joint = geometry.joints.at(jointIndex);
        const JointState& jointState = _jointStates.at(jointIndex);
        glm::vec3 position = _rotation * jointState.getPosition() + _translation;
        
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glm::quat parentRotation = (joint.parentIndex == -1) ? _rotation : _rotation * _jointStates.at(joint.parentIndex).getRotation();
        glm::vec3 rotationAxis = glm::axis(parentRotation);
        glRotatef(glm::degrees(glm::angle(parentRotation)), rotationAxis.x, rotationAxis.y, rotationAxis.z);
        float fanScale = directionSize * 0.75f;
        glScalef(fanScale, fanScale, fanScale);
        const int AXIS_COUNT = 3;
        for (int i = 0; i < AXIS_COUNT; i++) {
            if (joint.rotationMin[i] <= -PI + EPSILON && joint.rotationMax[i] >= PI - EPSILON) {
                continue; // unconstrained
            }
            glm::vec3 axis;
            axis[i] = 1.0f;
            
            glm::vec3 otherAxis;
            if (i == 0) {
                otherAxis.y = 1.0f;
            } else {
                otherAxis.x = 1.0f;
            }
            glColor4f(otherAxis.r, otherAxis.g, otherAxis.b, 0.75f);
        
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, 0.0f);
            const int FAN_SEGMENTS = 16;
            for (int j = 0; j < FAN_SEGMENTS; j++) {
                glm::vec3 rotated = glm::angleAxis(glm::mix(joint.rotationMin[i], joint.rotationMax[i],
                    (float)j / (FAN_SEGMENTS - 1)), axis) * otherAxis;
                glVertex3f(rotated.x, rotated.y, rotated.z);
            }
            glEnd();
        }
        glPopMatrix();
        
        renderOrientationDirections(position, _rotation * jointState.getRotation(), directionSize);
        jointIndex = joint.parentIndex;
        
    } while (jointIndex != -1 && geometry.joints.at(jointIndex).isFree);
    
    glLineWidth(1.0f);
}

void SkeletonModel::setHandPosition(int jointIndex, const glm::vec3& position, const glm::quat& rotation) {
    // this algorithm is from sample code from sixense
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    int elbowJointIndex = geometry.joints.at(jointIndex).parentIndex;
    if (elbowJointIndex == -1) {
        return;
    }
    int shoulderJointIndex = geometry.joints.at(elbowJointIndex).parentIndex;
    glm::vec3 shoulderPosition;
    if (!getJointPosition(shoulderJointIndex, shoulderPosition)) {
        return;
    }
    // precomputed lengths
    float scale = extractUniformScale(_scale);
    float upperArmLength = geometry.joints.at(elbowJointIndex).distanceToParent * scale;
    float lowerArmLength = geometry.joints.at(jointIndex).distanceToParent * scale;
    
    // first set wrist position
    glm::vec3 wristPosition = position;
    
    glm::vec3 shoulderToWrist = wristPosition - shoulderPosition;
    float distanceToWrist = glm::length(shoulderToWrist);
    
    // prevent gimbal lock
    if (distanceToWrist > upperArmLength + lowerArmLength - EPSILON) {
        distanceToWrist = upperArmLength + lowerArmLength - EPSILON;
        shoulderToWrist = glm::normalize(shoulderToWrist) * distanceToWrist;
        wristPosition = shoulderPosition + shoulderToWrist;
    }
    
    // cosine of angle from upper arm to hand vector 
    float cosA = (upperArmLength * upperArmLength + distanceToWrist * distanceToWrist - lowerArmLength * lowerArmLength) / 
        (2 * upperArmLength * distanceToWrist);
    float mid = upperArmLength * cosA;
    float height = sqrt(upperArmLength * upperArmLength + mid * mid - 2 * upperArmLength * mid * cosA);
    
    // direction of the elbow
    glm::vec3 handNormal = glm::cross(rotation * glm::vec3(0.0f, 1.0f, 0.0f), shoulderToWrist); // elbow rotating with wrist
    glm::vec3 relaxedNormal = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), shoulderToWrist); // elbow pointing straight down
    const float NORMAL_WEIGHT = 0.5f;
    glm::vec3 finalNormal = glm::mix(relaxedNormal, handNormal, NORMAL_WEIGHT);
    
    bool rightHand = (jointIndex == geometry.rightHandJointIndex);
    if (rightHand ? (finalNormal.y > 0.0f) : (finalNormal.y < 0.0f)) {
        finalNormal.y = 0.0f; // dont allow elbows to point inward (y is vertical axis)
    }
    
    glm::vec3 tangent = glm::normalize(glm::cross(shoulderToWrist, finalNormal));
    
    // ik solution
    glm::vec3 elbowPosition = shoulderPosition + glm::normalize(shoulderToWrist) * mid - tangent * height;
    glm::vec3 forwardVector(rightHand ? -1.0f : 1.0f, 0.0f, 0.0f);
    glm::quat shoulderRotation = rotationBetween(forwardVector, elbowPosition - shoulderPosition);

    JointState& shoulderState = _jointStates[shoulderJointIndex];
    shoulderState.setRotationInBindFrame(shoulderRotation, PALM_PRIORITY);
    
    JointState& elbowState = _jointStates[elbowJointIndex];
    elbowState.setRotationInBindFrame(rotationBetween(shoulderRotation * forwardVector, wristPosition - elbowPosition) * shoulderRotation, PALM_PRIORITY);
    
    JointState& handState = _jointStates[jointIndex];
    handState.setRotationInBindFrame(rotation, PALM_PRIORITY);
}
    
bool SkeletonModel::getLeftHandPosition(glm::vec3& position) const {
    return getJointPositionInWorldFrame(getLeftHandJointIndex(), position);
}

bool SkeletonModel::getRightHandPosition(glm::vec3& position) const {
    return getJointPositionInWorldFrame(getRightHandJointIndex(), position);
}

bool SkeletonModel::restoreLeftHandPosition(float fraction, float priority) {
    return restoreJointPosition(getLeftHandJointIndex(), fraction, priority);
}

bool SkeletonModel::getLeftShoulderPosition(glm::vec3& position) const {
    return getJointPositionInWorldFrame(getLastFreeJointIndex(getLeftHandJointIndex()), position);
}

float SkeletonModel::getLeftArmLength() const {
    return getLimbLength(getLeftHandJointIndex());
}

bool SkeletonModel::restoreRightHandPosition(float fraction, float priority) {
    return restoreJointPosition(getRightHandJointIndex(), fraction, priority);
}

bool SkeletonModel::getRightShoulderPosition(glm::vec3& position) const {
    return getJointPositionInWorldFrame(getLastFreeJointIndex(getRightHandJointIndex()), position);
}

float SkeletonModel::getRightArmLength() const {
    return getLimbLength(getRightHandJointIndex());
}

bool SkeletonModel::getHeadPosition(glm::vec3& headPosition) const {
    return isActive() && getJointPositionInWorldFrame(_geometry->getFBXGeometry().headJointIndex, headPosition);
}

bool SkeletonModel::getNeckPosition(glm::vec3& neckPosition) const {
    return isActive() && getJointPositionInWorldFrame(_geometry->getFBXGeometry().neckJointIndex, neckPosition);
}

bool SkeletonModel::getNeckParentRotationFromDefaultOrientation(glm::quat& neckParentRotation) const {
    if (!isActive()) {
        return false;
    }
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    if (geometry.neckJointIndex == -1) {
        return false;
    }
    int parentIndex = geometry.joints.at(geometry.neckJointIndex).parentIndex;
    glm::quat worldFrameRotation;
    if (getJointRotationInWorldFrame(parentIndex, worldFrameRotation)) {
        neckParentRotation = worldFrameRotation * _jointStates[parentIndex].getFBXJoint().inverseDefaultRotation;
        return true;
    }
    return false;
}

bool SkeletonModel::getEyePositions(glm::vec3& firstEyePosition, glm::vec3& secondEyePosition) const {
    if (!isActive()) {
        return false;
    }
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    if (getJointPositionInWorldFrame(geometry.leftEyeJointIndex, firstEyePosition) &&
            getJointPositionInWorldFrame(geometry.rightEyeJointIndex, secondEyePosition)) {
        return true;
    }
    // no eye joints; try to estimate based on head/neck joints
    glm::vec3 neckPosition, headPosition;
    if (getJointPositionInWorldFrame(geometry.neckJointIndex, neckPosition) &&
            getJointPositionInWorldFrame(geometry.headJointIndex, headPosition)) {
        const float EYE_PROPORTION = 0.6f;
        glm::vec3 baseEyePosition = glm::mix(neckPosition, headPosition, EYE_PROPORTION);
        glm::quat headRotation;
        getJointRotationInWorldFrame(geometry.headJointIndex, headRotation);
        const float EYES_FORWARD = 0.25f;
        const float EYE_SEPARATION = 0.1f;
        float headHeight = glm::distance(neckPosition, headPosition);
        firstEyePosition = baseEyePosition + headRotation * glm::vec3(EYE_SEPARATION, 0.0f, EYES_FORWARD) * headHeight;
        secondEyePosition = baseEyePosition + headRotation * glm::vec3(-EYE_SEPARATION, 0.0f, EYES_FORWARD) * headHeight;
        return true;
    }
    return false;
}

void SkeletonModel::renderRagdoll() {
    const int BALL_SUBDIVISIONS = 6;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glPushMatrix();

    Application::getInstance()->loadTranslatedViewMatrix(_translation);
    int numPoints = _ragdollPoints.size();
    float alpha = 0.3f;
    float radius1 = 0.008f;
    float radius2 = 0.01f;
    glm::vec3 simulationTranslation = getTranslationInSimulationFrame();
    for (int i = 0; i < numPoints; ++i) {
        glPushMatrix();
        // NOTE: ragdollPoints are in simulation-frame but we want them to be model-relative
        glm::vec3 position = _ragdollPoints[i]._position - simulationTranslation;
        glTranslatef(position.x, position.y, position.z);
        // draw each point as a yellow hexagon with black border
        glColor4f(0.0f, 0.0f, 0.0f, alpha);
        glutSolidSphere(radius2, BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);
        glColor4f(1.0f, 1.0f, 0.0f, alpha);
        glutSolidSphere(radius1, BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);
        glPopMatrix();
    }
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// virtual
void SkeletonModel::initRagdollPoints() {
    clearRagdollConstraintsAndPoints();
    _muscleConstraints.clear();

    initRagdollTransform();
    // one point for each joint
    int numStates = _jointStates.size();
    _ragdollPoints.fill(VerletPoint(), numStates);
    for (int i = 0; i < numStates; ++i) {
        const JointState& state = _jointStates.at(i);
        // _ragdollPoints start in model-frame
        _ragdollPoints[i].initPosition(state.getPosition());
    }
}

void SkeletonModel::buildRagdollConstraints() {
    // NOTE: the length of DistanceConstraints is computed and locked in at this time
    // so make sure the ragdoll positions are in a normal configuration before here.
    const int numPoints = _ragdollPoints.size();
    assert(numPoints == _jointStates.size());

    float minBone = FLT_MAX;
    float maxBone = -FLT_MAX;
    QMultiMap<int, int> families;
    for (int i = 0; i < numPoints; ++i) {
        const JointState& state = _jointStates.at(i);
        int parentIndex = state.getParentIndex();
        if (parentIndex == -1) {
            FixedConstraint* anchor = new FixedConstraint(&_translationInSimulationFrame, &(_ragdollPoints[i]));
            _fixedConstraints.push_back(anchor);
        } else { 
            DistanceConstraint* bone = new DistanceConstraint(&(_ragdollPoints[i]), &(_ragdollPoints[parentIndex]));
            bone->setDistance(state.getDistanceToParent());
            _boneConstraints.push_back(bone);
            families.insert(parentIndex, i);
        }
        float boneLength = glm::length(state.getPositionInParentFrame());
        if (boneLength > maxBone) {
            maxBone = boneLength;
        } else if (boneLength < minBone) {
            minBone = boneLength;
        }
    }
    // Joints that have multiple children effectively have rigid constraints between the children
    // in the parent frame, so we add DistanceConstraints between children in the same family.
    QMultiMap<int, int>::iterator itr = families.begin();
    while (itr != families.end()) {
        QList<int> children = families.values(itr.key());
        int numChildren = children.size();
        if (numChildren > 1) {
            for (int i = 1; i < numChildren; ++i) {
                DistanceConstraint* bone = new DistanceConstraint(&(_ragdollPoints[children[i-1]]), &(_ragdollPoints[children[i]]));
                _boneConstraints.push_back(bone);
            }
            if (numChildren > 2) {
                DistanceConstraint* bone = new DistanceConstraint(&(_ragdollPoints[children[numChildren-1]]), &(_ragdollPoints[children[0]]));
                _boneConstraints.push_back(bone);
            }
        }
        ++itr;
    }

    float MAX_STRENGTH = 0.6f;
    float MIN_STRENGTH = 0.05f;
    // each joint gets a MuscleConstraint to its parent
    for (int i = 1; i < numPoints; ++i) {
        const JointState& state = _jointStates.at(i);
        int p = state.getParentIndex();
        if (p == -1) {
            continue;
        }
        MuscleConstraint* constraint = new MuscleConstraint(&(_ragdollPoints[p]), &(_ragdollPoints[i]));
        _muscleConstraints.push_back(constraint);

        // Short joints are more susceptible to wiggle so we modulate the strength based on the joint's length: 
        // long = weak and short = strong.
        constraint->setIndices(p, i);
        float boneLength = glm::length(state.getPositionInParentFrame());

        float strength = MIN_STRENGTH + (MAX_STRENGTH - MIN_STRENGTH) * (maxBone - boneLength) / (maxBone - minBone);
        if (!families.contains(i)) {
            // Although muscles only pull on the children not parents, nevertheless those joints that have
            // parents AND children are more stable than joints at the end such as fingers.  For such joints we
            // bestow maximum strength which helps reduce wiggle.
            strength = MAX_MUSCLE_STRENGTH;
        }
        constraint->setStrength(strength);
    }
    
}

void SkeletonModel::updateVisibleJointStates() {
    if (_showTrueJointTransforms) {
        // no need to update visible transforms
        return;
    }
    QVector<glm::vec3> points;
    points.reserve(_jointStates.size());
    glm::quat invRotation = glm::inverse(_rotation);
    for (int i = 0; i < _jointStates.size(); i++) {
        JointState& state = _jointStates[i];
        points.push_back(_ragdollPoints[i]._position);

        // get the parent state (this is the state that we want to rotate)
        int parentIndex = state.getParentIndex();
        if (parentIndex == -1) {
            _jointStates[i].slaveVisibleTransform();
            continue;
        }
        JointState& parentState = _jointStates[parentIndex];

        // check the grand-parent index (for now we don't want to rotate any root states)
        int grandParentIndex = parentState.getParentIndex();
        if (grandParentIndex == -1) {
            continue;
        }

        // make sure state's visibleTransform is up to date
        const glm::mat4& parentTransform = parentState.getVisibleTransform();
        state.computeVisibleTransform(parentTransform);

        // we're looking for the rotation that moves visible bone parallel to ragdoll bone
        // rotationBetween(jointTip - jointPivot, shapeTip - shapePivot)
        // NOTE: points are in simulation-frame so rotate line segment into model-frame
        glm::quat delta = rotationBetween(state.getVisiblePosition() - extractTranslation(parentTransform), 
                invRotation * (points[i] - points[parentIndex]));

        // apply
        parentState.mixVisibleRotationDelta(delta, 0.01f);
        // update transforms
        parentState.computeVisibleTransform(_jointStates[grandParentIndex].getVisibleTransform());
        state.computeVisibleTransform(parentState.getVisibleTransform());
    }
}

// virtual 
void SkeletonModel::stepRagdollForward(float deltaTime) {
    setRagdollTransform(_translation, _rotation);
    Ragdoll::stepRagdollForward(deltaTime);
    updateMuscles();
    int numConstraints = _muscleConstraints.size();
    for (int i = 0; i < numConstraints; ++i) {
        _muscleConstraints[i]->enforce();
    }
}

float DENSITY_OF_WATER = 1000.0f; // kg/m^3
float MIN_JOINT_MASS = 1.0f;
float VERY_BIG_MASS = 1.0e6f;

// virtual
void SkeletonModel::buildShapes() {
    if (_geometry == NULL || _jointStates.isEmpty()) {
        return;
    }
    
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    if (geometry.joints.isEmpty()) {
        return;
    }

    initRagdollPoints();
    float massScale = getMassScale();

    float uniformScale = extractUniformScale(_scale);
    const int numStates = _jointStates.size();
    for (int i = 0; i < numStates; i++) {
        JointState& state = _jointStates[i];
        const FBXJoint& joint = state.getFBXJoint();
        float radius = uniformScale * joint.boneRadius;
        float halfHeight = 0.5f * uniformScale * joint.distanceToParent;
        Shape::Type type = joint.shapeType;
        if (i == 0 || (type == Shape::CAPSULE_SHAPE && halfHeight < EPSILON)) {
            // this shape is forced to be a sphere
            type = Shape::SPHERE_SHAPE;
        }
        if (radius < EPSILON) {
            type = Shape::UNKNOWN_SHAPE;
        }
        Shape* shape = NULL;
        int parentIndex = joint.parentIndex;
        if (type == Shape::SPHERE_SHAPE) {
            shape = new VerletSphereShape(radius, &(_ragdollPoints[i]));
            shape->setEntity(this);
            _ragdollPoints[i].setMass(massScale * glm::max(MIN_JOINT_MASS, DENSITY_OF_WATER * shape->getVolume()));
        } else if (type == Shape::CAPSULE_SHAPE) {
            assert(parentIndex != -1);
            shape = new VerletCapsuleShape(radius, &(_ragdollPoints[parentIndex]), &(_ragdollPoints[i]));
            shape->setEntity(this);
            _ragdollPoints[i].setMass(massScale * glm::max(MIN_JOINT_MASS, DENSITY_OF_WATER * shape->getVolume()));
        } 
        if (parentIndex != -1) {
            // always disable collisions between joint and its parent
            if (shape) {
                disableCollisions(i, parentIndex);
            }
        } else {
            // give the base joint a very large mass since it doesn't actually move
            // in the local-frame simulation (it defines the origin)
            _ragdollPoints[i].setMass(VERY_BIG_MASS);
        }
        _shapes.push_back(shape);
    }

    // This method moves the shapes to their default positions in Model frame.
    computeBoundingShape(geometry);

    // While the shapes are in their default position we disable collisions between
    // joints that are currently colliding.
    disableCurrentSelfCollisions();

    buildRagdollConstraints();

    // ... then move shapes back to current joint positions
    if (_ragdollPoints.size() == numStates) {
        int numStates = _jointStates.size();
        for (int i = 0; i < numStates; ++i) {
            // ragdollPoints start in model-frame
            _ragdollPoints[i].initPosition(_jointStates.at(i).getPosition());
        }
    }
    enforceRagdollConstraints();
}

void SkeletonModel::moveShapesTowardJoints(float deltaTime) {
    // KEEP: although we don't currently use this method we may eventually need it to help
    // unravel a skelton that has become tangled in its constraints.  So let's keep this
    // around for a while just in case.
    const int numStates = _jointStates.size();
    assert(_jointStates.size() == _ragdollPoints.size());
    if (_ragdollPoints.size() != numStates) {
        return;
    }

    // fraction = 0 means keep old position, = 1 means slave 100% to target position
    const float RAGDOLL_FOLLOWS_JOINTS_TIMESCALE = 0.05f;
    float fraction = glm::clamp(deltaTime / RAGDOLL_FOLLOWS_JOINTS_TIMESCALE, 0.0f, 1.0f);

    float oneMinusFraction = 1.0f - fraction; 
    glm::vec3 simulationTranslation = getTranslationInSimulationFrame();
    for (int i = 0; i < numStates; ++i) {
        // ragdollPoints are in simulation-frame but jointStates are in model-frame
        _ragdollPoints[i].initPosition(oneMinusFraction * _ragdollPoints[i]._position + 
                fraction * (simulationTranslation + _rotation * (_jointStates.at(i).getPosition())));
    }
}

void SkeletonModel::updateMuscles() {
    int numConstraints = _muscleConstraints.size();
    for (int i = 0; i < numConstraints; ++i) {
        MuscleConstraint* constraint = _muscleConstraints[i];
        int j = constraint->getParentIndex();
        int k = constraint->getChildIndex();
        assert(j != -1 && k != -1);
        // ragdollPoints are in simulation-frame but jointStates are in model-frame
        constraint->setChildOffset(_rotation * (_jointStates.at(k).getPosition() - _jointStates.at(j).getPosition()));
    }
}

void SkeletonModel::computeBoundingShape(const FBXGeometry& geometry) {
    // compute default joint transforms
    int numStates = _jointStates.size();
    QVector<glm::mat4> transforms;
    transforms.fill(glm::mat4(), numStates);

    // compute the default transforms and slam the ragdoll positions accordingly
    // (which puts the shapes where we want them)
    for (int i = 0; i < numStates; i++) {
        JointState& state = _jointStates[i];
        const FBXJoint& joint = state.getFBXJoint();
        int parentIndex = joint.parentIndex;
        if (parentIndex == -1) {
            transforms[i] = _jointStates[i].getTransform();
            _ragdollPoints[i].initPosition(extractTranslation(transforms[i]));
            continue;
        }
        
        glm::quat modifiedRotation = joint.preRotation * joint.rotation * joint.postRotation;    
        transforms[i] = transforms[parentIndex] * glm::translate(joint.translation) 
            * joint.preTransform * glm::mat4_cast(modifiedRotation) * joint.postTransform;
        // setting the ragdollPoints here slams the VerletShapes into their default positions
        _ragdollPoints[i].initPosition(extractTranslation(transforms[i]));
    }

    // compute bounding box that encloses all shapes
    Extents totalExtents;
    totalExtents.reset();
    totalExtents.addPoint(glm::vec3(0.0f));
    for (int i = 0; i < _shapes.size(); i++) {
        Shape* shape = _shapes[i];
        if (!shape) {
            continue;
        }
        // TODO: skip hand and arm shapes for bounding box calculation
        Extents shapeExtents;
        shapeExtents.reset();
        glm::vec3 localPosition = shape->getTranslation();
        int type = shape->getType();
        if (type == Shape::CAPSULE_SHAPE) {
            // add the two furthest surface points of the capsule
            CapsuleShape* capsule = static_cast<CapsuleShape*>(shape);
            glm::vec3 axis;
            capsule->computeNormalizedAxis(axis);
            float radius = capsule->getRadius();
            float halfHeight = capsule->getHalfHeight();
            axis = halfHeight * axis + glm::vec3(radius);

            shapeExtents.addPoint(localPosition + axis);
            shapeExtents.addPoint(localPosition - axis);
            totalExtents.addExtents(shapeExtents);
        } else if (type == Shape::SPHERE_SHAPE) {
            float radius = shape->getBoundingRadius();
            glm::vec3 axis = glm::vec3(radius);
            shapeExtents.addPoint(localPosition + axis);
            shapeExtents.addPoint(localPosition - axis);
            totalExtents.addExtents(shapeExtents);
        }
    }

    // compute bounding shape parameters
    // NOTE: we assume that the longest side of totalExtents is the yAxis...
    glm::vec3 diagonal = totalExtents.maximum - totalExtents.minimum;
    // ... and assume the radius is half the RMS of the X and Z sides:
    float capsuleRadius = 0.5f * sqrtf(0.5f * (diagonal.x * diagonal.x + diagonal.z * diagonal.z));
    _boundingShape.setRadius(capsuleRadius);
    _boundingShape.setHalfHeight(0.5f * diagonal.y - capsuleRadius);
    _boundingShapeLocalOffset = 0.5f * (totalExtents.maximum + totalExtents.minimum);
    _boundingRadius = 0.5f * glm::length(diagonal);
}

void SkeletonModel::resetShapePositionsToDefaultPose() {
    // DEBUG method.
    // Moves shapes to the joint default locations for debug visibility into
    // how the bounding shape is computed.

    if (!_geometry || _shapes.isEmpty()) {
        // geometry or joints have not yet been created
        return;
    }
    
    const FBXGeometry& geometry = _geometry->getFBXGeometry();
    if (geometry.joints.isEmpty()) {
        return;
    }

    // The shapes are moved to their default positions in computeBoundingShape().
    computeBoundingShape(geometry);

    // Then we move them into world frame for rendering at the Model's location.
    for (int i = 0; i < _shapes.size(); i++) {
        Shape* shape = _shapes[i];
        if (shape) {
            shape->setTranslation(_translation + _rotation * shape->getTranslation());
            shape->setRotation(_rotation * shape->getRotation());
        }
    }
    _boundingShape.setTranslation(_translation + _rotation * _boundingShapeLocalOffset);
    _boundingShape.setRotation(_rotation);
}

void SkeletonModel::renderBoundingCollisionShapes(float alpha) {
    const int BALL_SUBDIVISIONS = 10;
    if (_shapes.isEmpty()) {
        // the bounding shape has not been propery computed
        // so no need to render it
        return;
    }
    glPushMatrix();

    Application::getInstance()->loadTranslatedViewMatrix(_translation);

    // draw a blue sphere at the capsule endpoint
    glm::vec3 endPoint;
    _boundingShape.getEndPoint(endPoint);
    endPoint = endPoint - _translation;
    glTranslatef(endPoint.x, endPoint.y, endPoint.z);
    glColor4f(0.6f, 0.6f, 0.8f, alpha);
    glutSolidSphere(_boundingShape.getRadius(), BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);

    // draw a yellow sphere at the capsule startpoint
    glm::vec3 startPoint;
    _boundingShape.getStartPoint(startPoint);
    startPoint = startPoint - _translation;
    glm::vec3 axis = endPoint - startPoint;
    glTranslatef(-axis.x, -axis.y, -axis.z);
    glColor4f(0.8f, 0.8f, 0.6f, alpha);
    glutSolidSphere(_boundingShape.getRadius(), BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);

    // draw a green cylinder between the two points
    glm::vec3 origin(0.0f);
    glColor4f(0.6f, 0.8f, 0.6f, alpha);
    Avatar::renderJointConnectingCone( origin, axis, _boundingShape.getRadius(), _boundingShape.getRadius());

    glPopMatrix();
}

const int BALL_SUBDIVISIONS = 10;

// virtual
void SkeletonModel::renderJointCollisionShapes(float alpha) { 
    glPushMatrix();
    Application::getInstance()->loadTranslatedViewMatrix(_translation);
    glm::vec3 simulationTranslation = getTranslationInSimulationFrame();
    for (int i = 0; i < _shapes.size(); i++) { 
        Shape* shape = _shapes[i];
        if (!shape) { 
            continue;
        } 

        glPushMatrix();
        // shapes are stored in simulation-frame but we want position to be model-relative
        if (shape->getType() == Shape::SPHERE_SHAPE) { 
            glm::vec3 position = shape->getTranslation() - simulationTranslation;
            glTranslatef(position.x, position.y, position.z);
            // draw a grey sphere at shape position
            glColor4f(0.75f, 0.75f, 0.75f, alpha);
            glutSolidSphere(shape->getBoundingRadius(), BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);
        } else if (shape->getType() == Shape::CAPSULE_SHAPE) {
            CapsuleShape* capsule = static_cast<CapsuleShape*>(shape);

            // draw a blue sphere at the capsule endpoint                                         
            glm::vec3 endPoint;
            capsule->getEndPoint(endPoint);
            endPoint = endPoint - simulationTranslation;
            glTranslatef(endPoint.x, endPoint.y, endPoint.z);                                     
            glColor4f(0.6f, 0.6f, 0.8f, alpha);
            glutSolidSphere(capsule->getRadius(), BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);          

            // draw a yellow sphere at the capsule startpoint
            glm::vec3 startPoint;
            capsule->getStartPoint(startPoint);
            startPoint = startPoint - simulationTranslation;
            glm::vec3 axis = endPoint - startPoint;
            glTranslatef(-axis.x, -axis.y, -axis.z);
            glColor4f(0.8f, 0.8f, 0.6f, alpha);
            glutSolidSphere(capsule->getRadius(), BALL_SUBDIVISIONS, BALL_SUBDIVISIONS);

            // draw a green cylinder between the two points
            glm::vec3 origin(0.0f);
            glColor4f(0.6f, 0.8f, 0.6f, alpha);
            Avatar::renderJointConnectingCone( origin, axis, capsule->getRadius(), capsule->getRadius());
        }
        glPopMatrix();
    }
    glPopMatrix();
}

