//
//  OctreeInboundPacketProcessor.cpp
//  assignment-client/src/octree
//
//  Created by Brad Hefta-Gaub on 8/21/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <limits>
#include <PacketHeaders.h>
#include <PerfStat.h>

#include "OctreeServer.h"
#include "OctreeServerConsts.h"
#include "OctreeInboundPacketProcessor.h"

static QUuid DEFAULT_NODE_ID_REF;
const quint64 TOO_LONG_SINCE_LAST_NACK = 1 * USECS_PER_SECOND;

OctreeInboundPacketProcessor::OctreeInboundPacketProcessor(OctreeServer* myServer) :
    _myServer(myServer),
    _receivedPacketCount(0),
    _totalTransitTime(0),
    _totalProcessTime(0),
    _totalLockWaitTime(0),
    _totalElementsInPacket(0),
    _totalPackets(0),
    _lastNackTime(usecTimestampNow()),
    _shuttingDown(false)
{
}

void OctreeInboundPacketProcessor::resetStats() {
    _totalTransitTime = 0;
    _totalProcessTime = 0;
    _totalLockWaitTime = 0;
    _totalElementsInPacket = 0;
    _totalPackets = 0;
    _lastNackTime = usecTimestampNow();

    _singleSenderStats.clear();
}

unsigned long OctreeInboundPacketProcessor::getMaxWait() const {
    // calculate time until next sendNackPackets()
    quint64 nextNackTime = _lastNackTime + TOO_LONG_SINCE_LAST_NACK;
    quint64 now = usecTimestampNow();
    if (now >= nextNackTime) {
        return 0;
    }
    return (nextNackTime - now) / USECS_PER_MSEC + 1;
}

void OctreeInboundPacketProcessor::preProcess() {
    // check if it's time to send a nack. If yes, do so
    quint64 now = usecTimestampNow();
    if (now - _lastNackTime >= TOO_LONG_SINCE_LAST_NACK) {
        _lastNackTime = now;
        sendNackPackets();
    }
}

void OctreeInboundPacketProcessor::midProcess() {
    // check if it's time to send a nack. If yes, do so
    quint64 now = usecTimestampNow();
    if (now - _lastNackTime >= TOO_LONG_SINCE_LAST_NACK) {
        _lastNackTime = now;
        sendNackPackets();
    }
}

void OctreeInboundPacketProcessor::processPacket(const SharedNodePointer& sendingNode, const QByteArray& packet) {
    if (_shuttingDown) {
        qDebug() << "OctreeInboundPacketProcessor::processPacket() while shutting down... ignoring incoming packet";
        return;
    }

    bool debugProcessPacket = _myServer->wantsVerboseDebug();

    if (debugProcessPacket) {
        qDebug("OctreeInboundPacketProcessor::processPacket() packetData=%p packetLength=%d", &packet, packet.size());
    }

    int numBytesPacketHeader = numBytesForPacketHeader(packet);


    // Ask our tree subclass if it can handle the incoming packet...
    PacketType packetType = packetTypeForPacket(packet);
    if (_myServer->getOctree()->handlesEditPacketType(packetType)) {
        PerformanceWarning warn(debugProcessPacket, "processPacket KNOWN TYPE",debugProcessPacket);
        _receivedPacketCount++;
        
        const unsigned char* packetData = reinterpret_cast<const unsigned char*>(packet.data());

        unsigned short int sequence = (*((unsigned short int*)(packetData + numBytesPacketHeader)));
        quint64 sentAt = (*((quint64*)(packetData + numBytesPacketHeader + sizeof(sequence))));
        quint64 arrivedAt = usecTimestampNow();
        quint64 transitTime = arrivedAt - sentAt;
        int editsInPacket = 0;
        quint64 processTime = 0;
        quint64 lockWaitTime = 0;

        if (_myServer->wantsDebugReceiving()) {
            qDebug() << "PROCESSING THREAD: got '" << packetType << "' packet - " << _receivedPacketCount
                    << " command from client receivedBytes=" << packet.size()
                    << " sequence=" << sequence << " transitTime=" << transitTime << " usecs";
        }
        int atByte = numBytesPacketHeader + sizeof(sequence) + sizeof(sentAt);
        unsigned char* editData = (unsigned char*)&packetData[atByte];
        while (atByte < packet.size()) {
            int maxSize = packet.size() - atByte;

            if (debugProcessPacket) {
                qDebug("OctreeInboundPacketProcessor::processPacket() %c "
                       "packetData=%p packetLength=%d voxelData=%p atByte=%d maxSize=%d",
                        packetType, packetData, packet.size(), editData, atByte, maxSize);
            }

            quint64 startLock = usecTimestampNow();
            _myServer->getOctree()->lockForWrite();
            quint64 startProcess = usecTimestampNow();
            int editDataBytesRead = _myServer->getOctree()->processEditPacketData(packetType,
                                                                                  reinterpret_cast<const unsigned char*>(packet.data()),
                                                                                  packet.size(),
                                                                                  editData, maxSize, sendingNode);
            _myServer->getOctree()->unlock();
            quint64 endProcess = usecTimestampNow();

            editsInPacket++;
            quint64 thisProcessTime = endProcess - startProcess;
            quint64 thisLockWaitTime = startProcess - startLock;
            processTime += thisProcessTime;
            lockWaitTime += thisLockWaitTime;

            // skip to next voxel edit record in the packet
            editData += editDataBytesRead;
            atByte += editDataBytesRead;
        }

        if (debugProcessPacket) {
            qDebug("OctreeInboundPacketProcessor::processPacket() DONE LOOPING FOR %c "
                   "packetData=%p packetLength=%d voxelData=%p atByte=%d",
                    packetType, packetData, packet.size(), editData, atByte);
        }

        // Make sure our Node and NodeList knows we've heard from this node.
        QUuid& nodeUUID = DEFAULT_NODE_ID_REF;
        if (sendingNode) {
            sendingNode->setLastHeardMicrostamp(usecTimestampNow());
            nodeUUID = sendingNode->getUUID();
            if (debugProcessPacket) {
                qDebug() << "sender has uuid=" << nodeUUID;
            }
        } else {
            if (debugProcessPacket) {
                qDebug() << "sender has no known nodeUUID.";
            }
        }
        trackInboundPacket(nodeUUID, sequence, transitTime, editsInPacket, processTime, lockWaitTime);
    } else {
        qDebug("unknown packet ignored... packetType=%d", packetType);
    }
}

void OctreeInboundPacketProcessor::trackInboundPacket(const QUuid& nodeUUID, unsigned short int sequence, quint64 transitTime,
            int editsInPacket, quint64 processTime, quint64 lockWaitTime) {

    _totalTransitTime += transitTime;
    _totalProcessTime += processTime;
    _totalLockWaitTime += lockWaitTime;
    _totalElementsInPacket += editsInPacket;
    _totalPackets++;

    // find the individual senders stats and track them there too...
    // see if this is the first we've heard of this node...
    if (_singleSenderStats.find(nodeUUID) == _singleSenderStats.end()) {
        SingleSenderStats stats;
        stats.trackInboundPacket(sequence, transitTime, editsInPacket, processTime, lockWaitTime);
        _singleSenderStats[nodeUUID] = stats;
    } else {
        SingleSenderStats& stats = _singleSenderStats[nodeUUID];
        stats.trackInboundPacket(sequence, transitTime, editsInPacket, processTime, lockWaitTime);
    }
}

int OctreeInboundPacketProcessor::sendNackPackets() {
    int packetsSent = 0;

    if (_shuttingDown) {
        qDebug() << "OctreeInboundPacketProcessor::sendNackPackets() while shutting down... ignore";
        return packetsSent;
    }

    char packet[MAX_PACKET_SIZE];
    
    NodeToSenderStatsMapIterator i = _singleSenderStats.begin();
    while (i != _singleSenderStats.end()) {

        QUuid nodeUUID = i.key();
        SingleSenderStats nodeStats = i.value();

        // check if this node is still alive.  Remove its stats if it's dead.
        if (!isAlive(nodeUUID)) {
            i = _singleSenderStats.erase(i);
            continue;
        }

        // if there are packets from _node that are waiting to be processed,
        // don't send a NACK since the missing packets may be among those waiting packets.
        if (hasPacketsToProcessFrom(nodeUUID)) {
            i++;
            continue;
        }

        const SharedNodePointer& destinationNode = NodeList::getInstance()->getNodeHash().value(nodeUUID);

        // retrieve sequence number stats of node, prune its missing set
        SequenceNumberStats& sequenceNumberStats = nodeStats.getIncomingEditSequenceNumberStats();
        sequenceNumberStats.pruneMissingSet();
        
        // construct nack packet(s) for this node
        const QSet<unsigned short int>& missingSequenceNumbers = sequenceNumberStats.getMissingSet();
        int numSequenceNumbersAvailable = missingSequenceNumbers.size();
        QSet<unsigned short int>::const_iterator missingSequenceNumberIterator = missingSequenceNumbers.constBegin();
        while (numSequenceNumbersAvailable > 0) {

            char* dataAt = packet;
            int bytesRemaining = MAX_PACKET_SIZE;

            // pack header
            int numBytesPacketHeader = populatePacketHeader(packet, _myServer->getMyEditNackType());
            dataAt += numBytesPacketHeader;
            bytesRemaining -= numBytesPacketHeader;

            // calculate and pack the number of sequence numbers to nack
            int numSequenceNumbersRoomFor = (bytesRemaining - sizeof(uint16_t)) / sizeof(unsigned short int);
            uint16_t numSequenceNumbers = std::min(numSequenceNumbersAvailable, numSequenceNumbersRoomFor);
            uint16_t* numSequenceNumbersAt = (uint16_t*)dataAt;
            *numSequenceNumbersAt = numSequenceNumbers;
            dataAt += sizeof(uint16_t);

            // pack sequence numbers to nack
            for (uint16_t i = 0; i < numSequenceNumbers; i++) {
                unsigned short int* sequenceNumberAt = (unsigned short int*)dataAt;
                *sequenceNumberAt = *missingSequenceNumberIterator;
                dataAt += sizeof(unsigned short int);

                missingSequenceNumberIterator++;
            }
            numSequenceNumbersAvailable -= numSequenceNumbers;

            // send it
            NodeList::getInstance()->writeUnverifiedDatagram(packet, dataAt - packet, destinationNode);
            packetsSent++;
            
            qDebug() << "NACK Sent back to editor/client... destinationNode=" << nodeUUID;
        }
        i++;
    }
    return packetsSent;
}


SingleSenderStats::SingleSenderStats()
    : _totalTransitTime(0),
    _totalProcessTime(0),
    _totalLockWaitTime(0),
    _totalElementsInPacket(0),
    _totalPackets(0),
    _incomingEditSequenceNumberStats()
{

}

void SingleSenderStats::trackInboundPacket(unsigned short int incomingSequence, quint64 transitTime,
    int editsInPacket, quint64 processTime, quint64 lockWaitTime) {

    // track sequence number
    _incomingEditSequenceNumberStats.sequenceNumberReceived(incomingSequence);

    // update other stats
    _totalTransitTime += transitTime;
    _totalProcessTime += processTime;
    _totalLockWaitTime += lockWaitTime;
    _totalElementsInPacket += editsInPacket;
    _totalPackets++;
}
