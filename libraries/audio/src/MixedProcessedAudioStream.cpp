//
//  MixedProcessedAudioStream.cpp
//  libraries/audio/src
//
//  Created by Yixin Wang on 8/4/14.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MixedProcessedAudioStream.h"

MixedProcessedAudioStream ::MixedProcessedAudioStream (int numFrameSamples, int numFramesCapacity, bool dynamicJitterBuffers, int staticDesiredJitterBufferFrames, int maxFramesOverDesired, bool useStDevForJitterCalc)
    : InboundAudioStream(numFrameSamples, numFramesCapacity, dynamicJitterBuffers, staticDesiredJitterBufferFrames, maxFramesOverDesired, useStDevForJitterCalc)
{
}

void MixedProcessedAudioStream::outputFormatChanged(int outputFormatChannelCountTimesSampleRate) {
    _outputFormatChannelsTimesSampleRate = outputFormatChannelCountTimesSampleRate;
    int deviceOutputFrameSize = NETWORK_BUFFER_LENGTH_SAMPLES_PER_CHANNEL * _outputFormatChannelsTimesSampleRate / SAMPLE_RATE;
    _ringBuffer.resizeForFrameSize(deviceOutputFrameSize);
}

int MixedProcessedAudioStream::parseStreamProperties(PacketType type, const QByteArray& packetAfterSeqNum, int& numAudioSamples) {
    // mixed audio packets do not have any info between the seq num and the audio data.
    int numNetworkSamples = packetAfterSeqNum.size() / sizeof(int16_t);

    // since numAudioSamples is used to know how many samples to add for each dropped packet before this one,
    // we want to set it to the number of device audio samples since this stream contains device audio samples, not network samples.
    const int STEREO_DIVIDER = 2;
    numAudioSamples = numNetworkSamples * _outputFormatChannelsTimesSampleRate / (STEREO_DIVIDER * SAMPLE_RATE);

    return 0;
}

int MixedProcessedAudioStream::parseAudioData(PacketType type, const QByteArray& packetAfterStreamProperties, int numAudioSamples) {

    QByteArray outputBuffer;
    emit processSamples(packetAfterStreamProperties, outputBuffer);
    
    _ringBuffer.writeData(outputBuffer.data(), outputBuffer.size());

    return packetAfterStreamProperties.size();
}
