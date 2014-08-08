//
// bot_procedural.js
// hifi
//
// Created by Ben Arnold on 7/29/2013
//
// Copyright (c) 2014 HighFidelity, Inc. All rights reserved.
//
// This is an example script that demonstrates an NPC avatar.
//
//

//For procedural walk animation
Script.include("http://s3-us-west-1.amazonaws.com/highfidelity-public/scripts/proceduralAnimationAPI.js");

var procAnimAPI = new ProcAnimAPI();

function getRandomFloat(min, max) {
    return Math.random() * (max - min) + min;
}

function getRandomInt (min, max) {
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

function printVector(string, vector) {
    print(string + " " + vector.x + ", " + vector.y + ", " + vector.z);
}

var CHANCE_OF_MOVING = 0.005;
var CHANCE_OF_SOUND = 0.005;
var CHANCE_OF_HEAD_TURNING = 0.01;
var CHANCE_OF_BIG_MOVE = 1.0;

var isMoving = false;
var isTurningHead = false;
var isPlayingAudio = false; 

var X_MIN = 0.50;
var X_MAX = 15.60;
var Z_MIN = 0.50;
var Z_MAX = 15.10;
var Y_FEET = 0.0; 
var AVATAR_PELVIS_HEIGHT = 0.84;
var Y_PELVIS = Y_FEET + AVATAR_PELVIS_HEIGHT;
var MAX_PELVIS_DELTA = 2.5;

var MOVE_RANGE_SMALL = 3.0;
var MOVE_RANGE_BIG = 10.0;
var TURN_RANGE = 70.0;
var STOP_TOLERANCE = 0.05;
var MOVE_RATE = 0.05;
var TURN_RATE = 0.2;
var HEAD_TURN_RATE = 0.05;
var PITCH_RANGE = 15.0;
var YAW_RANGE = 35.0;

var firstPosition = { x: getRandomFloat(X_MIN, X_MAX), y: Y_PELVIS, z: getRandomFloat(Z_MIN, Z_MAX) };
var targetPosition =  { x: 0, y: 0, z: 0 };
var targetOrientation = { x: 0, y: 0, z: 0, w: 0 };
var currentOrientation = { x: 0, y: 0, z: 0, w: 0 };
var targetHeadPitch = 0.0;
var targetHeadYaw = 0.0;

var basePelvisHeight = 0.0;
var pelvisOscillatorPosition = 0.0;
var pelvisOscillatorVelocity = 0.0;

function clamp(val, min, max){
    return Math.max(min, Math.min(max, val))
}

//Array of all valid bot numbers
var validBotNumbers = [];

// right now we only use bot 63, since many other bots have messed up skeletons and LOD issues
var botNumber = 63;//getRandomInt(0, 99);

var newFaceFilePrefix = "ron";

var newBodyFilePrefix = "bot" + botNumber;

// set the face model fst using the bot number
// there is no need to change the body model - we're using the default
Avatar.faceModelURL = "https://s3-us-west-1.amazonaws.com/highfidelity-public/meshes/" + newFaceFilePrefix + ".fst";
Avatar.skeletonModelURL = "https://s3-us-west-1.amazonaws.com/highfidelity-public/meshes/" + newBodyFilePrefix + "_a.fst";
Avatar.billboardURL = "https://s3-us-west-1.amazonaws.com/highfidelity-public/meshes/billboards/bot" + botNumber + ".png";

Agent.isAvatar = true;
Agent.isListeningToAudioStream = true;

// change the avatar's position to the random one
Avatar.position = firstPosition;  
basePelvisHeight = firstPosition.y; 
printVector("New dancer, position = ", Avatar.position);

function loadSounds() {
    var sound_filenames = ["AB1.raw", "Anchorman2.raw", "B1.raw", "B1.raw", "Bale1.raw", "Bandcamp.raw",
    "Big1.raw", "Big2.raw", "Brian1.raw", "Buster1.raw", "CES1.raw", "CES2.raw", "CES3.raw", "CES4.raw", 
    "Carrie1.raw", "Carrie3.raw", "Charlotte1.raw", "EN1.raw", "EN2.raw", "EN3.raw", "Eugene1.raw", "Francesco1.raw",
    "Italian1.raw", "Japanese1.raw", "Leigh1.raw", "Lucille1.raw", "Lucille2.raw", "MeanGirls.raw", "Murray2.raw",
    "Nigel1.raw", "PennyLane.raw", "Pitt1.raw", "Ricardo.raw", "SN.raw", "Sake1.raw", "Samantha1.raw", "Samantha2.raw", 
    "Spicoli1.raw", "Supernatural.raw", "Swearengen1.raw", "TheDude.raw", "Tony.raw", "Triumph1.raw", "Uma1.raw",
    "Walken1.raw", "Walken2.raw", "Z1.raw", "Z2.raw"
    ];

    var footstep_filenames = ["FootstepW2Left-12db.wav", "FootstepW2Right-12db.wav", "FootstepW3Left-12db.wav", "FootstepW3Right-12db.wav", 
                        "FootstepW5Left-12db.wav", "FootstepW5Right-12db.wav"];

    var SOUND_BASE_URL = "https://s3-us-west-1.amazonaws.com/highfidelity-public/sounds/Cocktail+Party+Snippets/Raws/";

    var FOOTSTEP_BASE_URL = "http://highfidelity-public.s3-us-west-1.amazonaws.com/sounds/Footsteps/";

    for (var i = 0; i < sound_filenames.length; i++) {
        sounds.push(new Sound(SOUND_BASE_URL + sound_filenames[i]));
    }

    for (var i = 0; i < footstep_filenames.length; i++) {
        footstepSounds.push(new Sound(FOOTSTEP_BASE_URL + footstep_filenames[i]));
    }
}

var sounds = [];
var footstepSounds = [];
loadSounds();


function playRandomSound() {
    if (!Agent.isPlayingAvatarSound) {
        var whichSound = Math.floor((Math.random() * sounds.length));
        Agent.playAvatarSound(sounds[whichSound]);
    }
}

function playRandomFootstepSound() {

    var whichSound = Math.floor((Math.random() * footstepSounds.length));
    var options = new AudioInjectionOptions();
	options.position = Avatar.position;
	options.volume = 1.0;
	Audio.playSound(footstepSounds[whichSound], options);

}

// ************************************ Facial Animation **********************************
var allBlendShapes = [];
var targetBlendCoefficient = [];
var currentBlendCoefficient = [];

//Blendshape constructor
function addBlendshapeToPose(pose, shapeIndex, val) {
    var index = pose.blendShapes.length;
    pose.blendShapes[index] = {shapeIndex: shapeIndex, val: val };
}
//The mood of the avatar, determines face. 0 = happy, 1 = angry, 2 = sad.

//Randomly pick avatar mood. 80% happy, 10% mad 10% sad
var randMood = Math.floor(Math.random() * 11);
var avatarMood;
if (randMood == 0) {
    avatarMood = 1;
} else if (randMood == 2) {
    avatarMood = 2;
} else {
    avatarMood = 0;
}

var currentExpression = -1;
//Face pose constructor
var happyPoses = [];

happyPoses[0] = {blendShapes: []};
addBlendshapeToPose(happyPoses[0], 28, 0.7); //MouthSmile_L
addBlendshapeToPose(happyPoses[0], 29, 0.7); //MouthSmile_R

happyPoses[1] = {blendShapes: []};
addBlendshapeToPose(happyPoses[1], 28, 1.0); //MouthSmile_L
addBlendshapeToPose(happyPoses[1], 29, 1.0); //MouthSmile_R
addBlendshapeToPose(happyPoses[1], 21, 0.2); //JawOpen

happyPoses[2] = {blendShapes: []};
addBlendshapeToPose(happyPoses[2], 28, 1.0); //MouthSmile_L
addBlendshapeToPose(happyPoses[2], 29, 1.0); //MouthSmile_R
addBlendshapeToPose(happyPoses[2], 21, 0.5); //JawOpen
addBlendshapeToPose(happyPoses[2], 46, 1.0); //CheekSquint_L
addBlendshapeToPose(happyPoses[2], 47, 1.0); //CheekSquint_R
addBlendshapeToPose(happyPoses[2], 17, 1.0); //BrowsU_L
addBlendshapeToPose(happyPoses[2], 18, 1.0); //BrowsU_R

var angryPoses = [];

angryPoses[0] = {blendShapes: []};
addBlendshapeToPose(angryPoses[0], 26, 0.6); //MouthFrown_L
addBlendshapeToPose(angryPoses[0], 27, 0.6); //MouthFrown_R
addBlendshapeToPose(angryPoses[0], 14, 0.6); //BrowsD_L
addBlendshapeToPose(angryPoses[0], 15, 0.6); //BrowsD_R

angryPoses[1] = {blendShapes: []};
addBlendshapeToPose(angryPoses[1], 26, 0.9); //MouthFrown_L
addBlendshapeToPose(angryPoses[1], 27, 0.9); //MouthFrown_R
addBlendshapeToPose(angryPoses[1], 14, 0.9); //BrowsD_L
addBlendshapeToPose(angryPoses[1], 15, 0.9); //BrowsD_R

angryPoses[2] = {blendShapes: []};
addBlendshapeToPose(angryPoses[2], 26, 1.0); //MouthFrown_L
addBlendshapeToPose(angryPoses[2], 27, 1.0); //MouthFrown_R
addBlendshapeToPose(angryPoses[2], 14, 1.0); //BrowsD_L
addBlendshapeToPose(angryPoses[2], 15, 1.0); //BrowsD_R
addBlendshapeToPose(angryPoses[2], 21, 0.5); //JawOpen
addBlendshapeToPose(angryPoses[2], 46, 1.0); //CheekSquint_L
addBlendshapeToPose(angryPoses[2], 47, 1.0); //CheekSquint_R

var sadPoses = [];

sadPoses[0] = {blendShapes: []};
addBlendshapeToPose(sadPoses[0], 26, 0.6); //MouthFrown_L
addBlendshapeToPose(sadPoses[0], 27, 0.6); //MouthFrown_R
addBlendshapeToPose(sadPoses[0], 16, 0.2); //BrowsU_C
addBlendshapeToPose(sadPoses[0], 2, 0.6); //EyeSquint_L
addBlendshapeToPose(sadPoses[0], 3, 0.6); //EyeSquint_R

sadPoses[1] = {blendShapes: []};
addBlendshapeToPose(sadPoses[1], 26, 0.9); //MouthFrown_L
addBlendshapeToPose(sadPoses[1], 27, 0.9); //MouthFrown_R
addBlendshapeToPose(sadPoses[1], 16, 0.6); //BrowsU_C
addBlendshapeToPose(sadPoses[1], 2, 0.9); //EyeSquint_L
addBlendshapeToPose(sadPoses[1], 3, 0.9); //EyeSquint_R

sadPoses[2] = {blendShapes: []};
addBlendshapeToPose(sadPoses[2], 26, 1.0); //MouthFrown_L
addBlendshapeToPose(sadPoses[2], 27, 1.0); //MouthFrown_R
addBlendshapeToPose(sadPoses[2], 16, 0.1); //BrowsU_C
addBlendshapeToPose(sadPoses[2], 2, 1.0); //EyeSquint_L
addBlendshapeToPose(sadPoses[2], 3, 1.0); //EyeSquint_R
addBlendshapeToPose(sadPoses[2], 21, 0.3); //JawOpen

var facePoses = [];
facePoses[0] = happyPoses;
facePoses[1] = angryPoses;
facePoses[2] = sadPoses;


function addBlendShape(s) {
    allBlendShapes[allBlendShapes.length] = s;
}

//It is imperative that the following blendshapes are all present and are in the correct order
addBlendShape("EyeBlink_L"); //0
addBlendShape("EyeBlink_R"); //1
addBlendShape("EyeSquint_L"); //2
addBlendShape("EyeSquint_R"); //3
addBlendShape("EyeDown_L"); //4
addBlendShape("EyeDown_R"); //5
addBlendShape("EyeIn_L"); //6
addBlendShape("EyeIn_R"); //7
addBlendShape("EyeOpen_L"); //8
addBlendShape("EyeOpen_R"); //9
addBlendShape("EyeOut_L"); //10
addBlendShape("EyeOut_R"); //11
addBlendShape("EyeUp_L"); //12
addBlendShape("EyeUp_R"); //13
addBlendShape("BrowsD_L"); //14
addBlendShape("BrowsD_R"); //15
addBlendShape("BrowsU_C"); //16
addBlendShape("BrowsU_L"); //17
addBlendShape("BrowsU_R"); //18
addBlendShape("JawFwd"); //19
addBlendShape("JawLeft"); //20
addBlendShape("JawOpen"); //21
addBlendShape("JawChew"); //22
addBlendShape("JawRight"); //23
addBlendShape("MouthLeft"); //24
addBlendShape("MouthRight"); //25
addBlendShape("MouthFrown_L"); //26
addBlendShape("MouthFrown_R"); //27
addBlendShape("MouthSmile_L"); //28
addBlendShape("MouthSmile_R"); //29
addBlendShape("MouthDimple_L"); //30
addBlendShape("MouthDimple_R"); //31
addBlendShape("LipsStretch_L"); //32
addBlendShape("LipsStretch_R"); //33
addBlendShape("LipsUpperClose"); //34
addBlendShape("LipsLowerClose"); //35
addBlendShape("LipsUpperUp"); //36
addBlendShape("LipsLowerDown"); //37
addBlendShape("LipsUpperOpen"); //38
addBlendShape("LipsLowerOpen"); //39
addBlendShape("LipsFunnel"); //40
addBlendShape("LipsPucker"); //41
addBlendShape("ChinLowerRaise"); //42
addBlendShape("ChinUpperRaise"); //43
addBlendShape("Sneer"); //44
addBlendShape("Puff"); //45
addBlendShape("CheekSquint_L"); //46
addBlendShape("CheekSquint_R"); //47

for (var i = 0; i < allBlendShapes.length; i++) {
    targetBlendCoefficient[i] = 0;
    currentBlendCoefficient[i] = 0;
}

function setRandomExpression() {

    //Clear all expression data for current expression
    if (currentExpression != -1) {
        var expression = facePoses[avatarMood][currentExpression];
        for (var i = 0; i < expression.blendShapes.length; i++) {
            targetBlendCoefficient[expression.blendShapes[i].shapeIndex] = 0.0;
        }
    }
    //Get a new current expression
    currentExpression = Math.floor(Math.random() * facePoses[avatarMood].length);
    var expression = facePoses[avatarMood][currentExpression];
    for (var i = 0; i < expression.blendShapes.length; i++) {
        targetBlendCoefficient[expression.blendShapes[i].shapeIndex] = expression.blendShapes[i].val;
    }
}

var expressionChangeSpeed = 0.1;
function updateBlendShapes(deltaTime) {
  
    for (var i = 0; i < allBlendShapes.length; i++) {
        currentBlendCoefficient[i] += (targetBlendCoefficient[i] - currentBlendCoefficient[i]) * expressionChangeSpeed;
        Avatar.setBlendshape(allBlendShapes[i], currentBlendCoefficient[i]);
    }
}

var BLINK_SPEED = 0.15;
var CHANCE_TO_BLINK = 0.0025;
var MAX_BLINK = 0.85;
var blink = 0.0;
var isBlinking = false;
function updateBlinking(deltaTime) {
    if (isBlinking == false) {
        if (Math.random() < CHANCE_TO_BLINK) {
            isBlinking = true;
        } else {
            blink -= BLINK_SPEED;
            if (blink < 0.0) blink = 0.0;
        }
    } else {
        blink += BLINK_SPEED;
        if (blink > MAX_BLINK) {
            blink = MAX_BLINK;
            isBlinking = false;
        }
    }
    
    currentBlendCoefficient[0] = blink;
    currentBlendCoefficient[1] = blink;
    targetBlendCoefficient[0] = blink;
    targetBlendCoefficient[1] = blink;
}

// *************************************************************************************

//Procedural walk animation using two keyframes
//We use a separate array for front and back joints
//Pitch, yaw, and roll for the joints
var rightAngles = [];
var leftAngles = [];
//for non mirrored joints such as the spine
var middleAngles = [];

//Actual joint mappings
var SHOULDER_JOINT_NUMBER = 15; 
var ELBOW_JOINT_NUMBER = 16;
var JOINT_R_HIP = 1;
var JOINT_R_KNEE = 2;
var JOINT_L_HIP = 6;
var JOINT_L_KNEE = 7;
var JOINT_R_ARM = 15;
var JOINT_R_FOREARM = 16;
var JOINT_L_ARM = 39;
var JOINT_L_FOREARM = 40;
var JOINT_SPINE = 11;
var JOINT_R_FOOT = 3;
var JOINT_L_FOOT = 8;
var JOINT_R_TOE = 4;
var JOINT_L_TOE = 9;

// ******************************* Animation Is Defined Below *************************************

var NUM_FRAMES = 2;
for (var i = 0; i < NUM_FRAMES; i++) {
    rightAngles[i] = [];
    leftAngles[i] = [];
    middleAngles[i] = [];
}
//Joint order for actual joint mappings, should be interleaved R,L,R,L,...S,S,S for R = right, L = left, S = single
var JOINT_ORDER = [];
//*** right / left joints ***
var HIP = 0;
JOINT_ORDER.push(JOINT_R_HIP);
JOINT_ORDER.push(JOINT_L_HIP);
var KNEE = 1;
JOINT_ORDER.push(JOINT_R_KNEE);
JOINT_ORDER.push(JOINT_L_KNEE);
var ARM = 2;
JOINT_ORDER.push(JOINT_R_ARM);
JOINT_ORDER.push(JOINT_L_ARM);
var FOREARM = 3;
JOINT_ORDER.push(JOINT_R_FOREARM);
JOINT_ORDER.push(JOINT_L_FOREARM);
var FOOT = 4;
JOINT_ORDER.push(JOINT_R_FOOT);
JOINT_ORDER.push(JOINT_L_FOOT);
var TOE = 5;
JOINT_ORDER.push(JOINT_R_TOE);
JOINT_ORDER.push(JOINT_L_TOE);
//*** middle joints ***
var SPINE = 0;
JOINT_ORDER.push(JOINT_SPINE);

//We have to store the angles so we can invert yaw and roll when making the animation
//symmetrical

//Front refers to leg, not arm.
//Legs Extending
rightAngles[0][HIP] = [30.0, 0.0, 8.0];
rightAngles[0][KNEE] = [-15.0, 0.0, 0.0];
rightAngles[0][ARM] = [85.0, -25.0, 0.0];
rightAngles[0][FOREARM] = [0.0, 0.0, -15.0];
rightAngles[0][FOOT] = [0.0, 0.0, 0.0];
rightAngles[0][TOE] = [0.0, 0.0, 0.0];

leftAngles[0][HIP] = [-15, 0.0, 8.0];
leftAngles[0][KNEE] = [-26, 0.0, 0.0];
leftAngles[0][ARM] = [85.0, 20.0, 0.0];
leftAngles[0][FOREARM] = [10.0, 0.0, -25.0];
leftAngles[0][FOOT] = [-13.0, 0.0, 0.0];
leftAngles[0][TOE] = [34.0, 0.0, 0.0];

middleAngles[0][SPINE] = [0.0, -15.0, 5.0];

//Legs Passing
rightAngles[1][HIP] = [6.0, 0.0, 8.0];
rightAngles[1][KNEE] = [-12.0, 0.0, 0.0];
rightAngles[1][ARM] = [85.0, 0.0, 0.0];
rightAngles[1][FOREARM] = [0.0, 0.0, -15.0];
rightAngles[1][FOOT] = [6.0, -8.0, 0.0];
rightAngles[1][TOE] = [0.0, 0.0, 0.0];

leftAngles[1][HIP] = [10.0, 0.0, 8.0];
leftAngles[1][KNEE] = [-60.0, 0.0, 0.0];
leftAngles[1][ARM] = [85.0, 0.0, 0.0];
leftAngles[1][FOREARM] = [0.0, 0.0, -15.0];
leftAngles[1][FOOT] = [0.0, 0.0, 0.0];
leftAngles[1][TOE] = [0.0, 0.0, 0.0];

middleAngles[1][SPINE] = [0.0, 0.0, 0.0];

//Actual keyframes for the animation
var walkKeyFrames = procAnimAPI.generateKeyframes(rightAngles, leftAngles, middleAngles, NUM_FRAMES);

// ******************************* Animation Is Defined Above *************************************

// ********************************** Standing Key Frame ******************************************
//We don't have to do any mirroring or anything, since this is just a single pose.
var rightQuats = [];
var leftQuats = [];
var middleQuats = [];

rightQuats[HIP] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 7.0);
rightQuats[KNEE] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 0.0);
rightQuats[ARM] = Quat.fromPitchYawRollDegrees(85.0, 0.0, 0.0);
rightQuats[FOREARM] = Quat.fromPitchYawRollDegrees(0.0, 0.0, -10.0);
rightQuats[FOOT] = Quat.fromPitchYawRollDegrees(0.0, -8.0, 0.0);
rightQuats[TOE] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 0.0);

leftQuats[HIP] = Quat.fromPitchYawRollDegrees(0, 0.0, -7.0);
leftQuats[KNEE] = Quat.fromPitchYawRollDegrees(0, 0.0, 0.0);
leftQuats[ARM] = Quat.fromPitchYawRollDegrees(85.0, 0.0, 0.0);
leftQuats[FOREARM] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 10.0);
leftQuats[FOOT] = Quat.fromPitchYawRollDegrees(0.0, 8.0, 0.0);
leftQuats[TOE] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 0.0);

middleQuats[SPINE] = Quat.fromPitchYawRollDegrees(0.0, 0.0, 0.0);

var standingKeyFrame = new procAnimAPI.KeyFrame(rightQuats, leftQuats, middleQuats);

// ************************************************************************************************


var currentFrame = 0;

var walkTime = 0.0;

var walkWheelRadius = 0.5;
var walkWheelRate = 2.0 * 3.141592 * walkWheelRadius / 8.0;

var avatarAcceleration = 0.75;
var avatarVelocity = 0.0;
var avatarMaxVelocity = 1.4;

function handleAnimation(deltaTime) {
  
    updateBlinking(deltaTime);
    updateBlendShapes(deltaTime);
  
    if (Math.random() < 0.01) {
        setRandomExpression();
    }
  
   if (avatarVelocity == 0.0) {
       walkTime = 0.0;
       currentFrame = 0;
   } else {
        walkTime += avatarVelocity * deltaTime;
        if (walkTime > walkWheelRate) {
            walkTime = 0.0;
            currentFrame++;
            if (currentFrame % 2 == 1) {
                playRandomFootstepSound();
            }
            if (currentFrame > 3) {
                currentFrame = 0;
            }
        } 
    }

    var frame = walkKeyFrames[currentFrame];
   
    var walkInterp = walkTime / walkWheelRate;
    var animInterp = avatarVelocity / (avatarMaxVelocity / 1.3);
    if (animInterp > 1.0) animInterp = 1.0;
   
    for (var i = 0; i < JOINT_ORDER.length; i++) {
        var walkJoint = procAnimAPI.deCasteljau(frame.rotations[i], frame.nextFrame.rotations[i], frame.controlPoints[i][0], frame.controlPoints[i][1], walkInterp);
        var standJoint = standingKeyFrame.rotations[i];
        var finalJoint = Quat.mix(standJoint, walkJoint, animInterp);
        Avatar.setJointData(JOINT_ORDER[i], finalJoint);
    }
}

function jumpWithLoudness(deltaTime) {
    // potentially change pelvis height depending on trailing average loudness

    pelvisOscillatorVelocity += deltaTime * Agent.lastReceivedAudioLoudness * 700.0 ;

    pelvisOscillatorVelocity -= pelvisOscillatorPosition * 0.75;
    pelvisOscillatorVelocity *= 0.97;
    pelvisOscillatorPosition += deltaTime * pelvisOscillatorVelocity;
    Avatar.headPitch = pelvisOscillatorPosition * 60.0;

    var pelvisPosition = Avatar.position;
    pelvisPosition.y = (Y_PELVIS - 0.35) + pelvisOscillatorPosition;

    if (pelvisPosition.y < Y_PELVIS) {
        pelvisPosition.y = Y_PELVIS;
    } else if (pelvisPosition.y > Y_PELVIS + 1.0) {
        pelvisPosition.y = Y_PELVIS + 1.0;
    }

    Avatar.position = pelvisPosition;
}

var forcedMove = false;

var wasMovingLastFrame = false;

function handleHeadTurn() {
    if (!isTurningHead && (Math.random() < CHANCE_OF_HEAD_TURNING)) {
        targetHeadPitch = getRandomFloat(-PITCH_RANGE, PITCH_RANGE);
        targetHeadYaw = getRandomFloat(-YAW_RANGE, YAW_RANGE);
        isTurningHead = true;
    } else {
        Avatar.headPitch = Avatar.headPitch + (targetHeadPitch - Avatar.headPitch) * HEAD_TURN_RATE;
        Avatar.headYaw = Avatar.headYaw + (targetHeadYaw - Avatar.headYaw) * HEAD_TURN_RATE;
        if (Math.abs(Avatar.headPitch - targetHeadPitch) < STOP_TOLERANCE && 
            Math.abs(Avatar.headYaw - targetHeadYaw) < STOP_TOLERANCE) {
            isTurningHead = false;
        }
    }
}

function stopWalking() {
    avatarVelocity = 0.0;
    isMoving = false;
}

var MAX_ATTEMPTS = 40;
function handleWalking(deltaTime) {

    if (forcedMove || (!isMoving && Math.random() < CHANCE_OF_MOVING)) {
        // Set new target location

        var moveRange;
        if (Math.random() < CHANCE_OF_BIG_MOVE) {
            moveRange = MOVE_RANGE_BIG;
        } else {
            moveRange = MOVE_RANGE_SMALL;
        }   
        
        //Keep trying new orientations if the desired target location is out of bounds
        var attempts = 0;
        do {
            targetOrientation = Quat.multiply(Avatar.orientation, Quat.angleAxis(getRandomFloat(-TURN_RANGE, TURN_RANGE), { x:0, y:1, z:0 }));
            var front = Quat.getFront(targetOrientation);
            
            targetPosition = Vec3.sum(Avatar.position, Vec3.multiply(front, getRandomFloat(0.0, moveRange)));
        }
        while ((targetPosition.x < X_MIN || targetPosition.x > X_MAX || targetPosition.z < Z_MIN || targetPosition.z > Z_MAX) 
        && attempts < MAX_ATTEMPTS);
            
        targetPosition.x = clamp(targetPosition.x, X_MIN, X_MAX);
        targetPosition.z = clamp(targetPosition.z, Z_MIN, Z_MAX);
        targetPosition.y = Y_PELVIS;

        wasMovingLastFrame = true;
        isMoving = true;
        forcedMove = false;
    } else if (isMoving) { 

        var targetVector = Vec3.subtract(targetPosition, Avatar.position);
        var distance = Vec3.length(targetVector);
        if (distance <= avatarVelocity * deltaTime) {
            Avatar.position = targetPosition;
            stopWalking(); 
        } else {
            var direction = Vec3.normalize(targetVector);
            //Figure out if we should be slowing down
            var t = avatarVelocity / avatarAcceleration;
            var d = (avatarVelocity / 2.0) * t;
            if (distance < d) { 
                avatarVelocity -= avatarAcceleration * deltaTime;
                if (avatarVelocity <= 0) {
                    stopWalking(); 
                }
            } else {    
                avatarVelocity += avatarAcceleration * deltaTime;
                if (avatarVelocity > avatarMaxVelocity) avatarVelocity = avatarMaxVelocity;
            }
            Avatar.position = Vec3.sum(Avatar.position, Vec3.multiply(direction, avatarVelocity * deltaTime));
            Avatar.orientation = Quat.mix(Avatar.orientation, targetOrientation, TURN_RATE);
            
            wasMovingLastFrame = true; 

        }
    }
}

function handleTalking() {
    if (Math.random() < CHANCE_OF_SOUND) {
        playRandomSound();
    }
}

function changePelvisHeight(newHeight) {
    var newPosition = Avatar.position;
    newPosition.y = newHeight;
    Avatar.position = newPosition;
}

function updateBehavior(deltaTime) {

    if (AvatarList.containsAvatarWithDisplayName("mrdj")) {
        if (wasMovingLastFrame) {
            isMoving = false;
        }

        // we have a DJ, shouldn't we be dancing?
        jumpWithLoudness(deltaTime);
    } else {

        // no DJ, let's just chill on the dancefloor - randomly walking and talking
        handleHeadTurn();
        handleAnimation(deltaTime);
        handleWalking(deltaTime);
        handleTalking();   
    }
}

Script.update.connect(updateBehavior);