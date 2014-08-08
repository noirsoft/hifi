//
//  OctreeServer.cpp
//  assignment-client/src/octree
//
//  Created by Brad Hefta-Gaub on 9/16/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>

#include <time.h>

#include <AccountManager.h>
#include <HTTPConnection.h>
#include <Logging.h>
#include <UUID.h>

#include "../AssignmentClient.h"

#include "OctreeServer.h"
#include "OctreeServerConsts.h"

OctreeServer* OctreeServer::_instance = NULL;
int OctreeServer::_clientCount = 0;
const int MOVING_AVERAGE_SAMPLE_COUNTS = 1000000;

float OctreeServer::SKIP_TIME = -1.0f; // use this for trackXXXTime() calls for non-times

SimpleMovingAverage OctreeServer::_averageLoopTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageInsideTime(MOVING_AVERAGE_SAMPLE_COUNTS);

SimpleMovingAverage OctreeServer::_averageEncodeTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageShortEncodeTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageLongEncodeTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageExtraLongEncodeTime(MOVING_AVERAGE_SAMPLE_COUNTS);
int OctreeServer::_extraLongEncode = 0;
int OctreeServer::_longEncode = 0;
int OctreeServer::_shortEncode = 0;
int OctreeServer::_noEncode = 0;

SimpleMovingAverage OctreeServer::_averageTreeWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageTreeShortWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageTreeLongWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageTreeExtraLongWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
int OctreeServer::_extraLongTreeWait = 0;
int OctreeServer::_longTreeWait = 0;
int OctreeServer::_shortTreeWait = 0;
int OctreeServer::_noTreeWait = 0;

SimpleMovingAverage OctreeServer::_averageNodeWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);

SimpleMovingAverage OctreeServer::_averageCompressAndWriteTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageShortCompressTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageLongCompressTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageExtraLongCompressTime(MOVING_AVERAGE_SAMPLE_COUNTS);
int OctreeServer::_extraLongCompress = 0;
int OctreeServer::_longCompress = 0;
int OctreeServer::_shortCompress = 0;
int OctreeServer::_noCompress = 0;

SimpleMovingAverage OctreeServer::_averagePacketSendingTime(MOVING_AVERAGE_SAMPLE_COUNTS);
int OctreeServer::_noSend = 0;

SimpleMovingAverage OctreeServer::_averageProcessWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageProcessShortWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageProcessLongWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
SimpleMovingAverage OctreeServer::_averageProcessExtraLongWaitTime(MOVING_AVERAGE_SAMPLE_COUNTS);
int OctreeServer::_extraLongProcessWait = 0;
int OctreeServer::_longProcessWait = 0;
int OctreeServer::_shortProcessWait = 0;
int OctreeServer::_noProcessWait = 0;


void OctreeServer::resetSendingStats() {
    _averageLoopTime.reset();

    _averageEncodeTime.reset();
    _averageShortEncodeTime.reset();
    _averageLongEncodeTime.reset();
    _averageExtraLongEncodeTime.reset();
    _extraLongEncode = 0;
    _longEncode = 0;
    _shortEncode = 0;
    _noEncode = 0;

    _averageInsideTime.reset();
    _averageTreeWaitTime.reset();
    _averageTreeShortWaitTime.reset();
    _averageTreeLongWaitTime.reset();
    _averageTreeExtraLongWaitTime.reset();
    _extraLongTreeWait = 0;
    _longTreeWait = 0;
    _shortTreeWait = 0;
    _noTreeWait = 0;

    _averageNodeWaitTime.reset();

    _averageCompressAndWriteTime.reset();
    _averageShortCompressTime.reset();
    _averageLongCompressTime.reset();
    _averageExtraLongCompressTime.reset();
    _extraLongCompress = 0;
    _longCompress = 0;
    _shortCompress = 0;
    _noCompress = 0;

    _averagePacketSendingTime.reset();
    _noSend = 0;

    _averageProcessWaitTime.reset();
    _averageProcessShortWaitTime.reset();
    _averageProcessLongWaitTime.reset();
    _averageProcessExtraLongWaitTime.reset();
    _extraLongProcessWait = 0;
    _longProcessWait = 0;
    _shortProcessWait = 0;
    _noProcessWait = 0;
}

void OctreeServer::trackEncodeTime(float time) { 
    const float MAX_SHORT_TIME = 10.0f;
    const float MAX_LONG_TIME = 100.0f;

    if (time == SKIP_TIME) {
        _noEncode++;
        time = 0.0f;
    } else if (time <= MAX_SHORT_TIME) {
        _shortEncode++;
        _averageShortEncodeTime.updateAverage(time);
    } else if (time <= MAX_LONG_TIME) {
        _longEncode++;
        _averageLongEncodeTime.updateAverage(time);
    } else {
        _extraLongEncode++;
        _averageExtraLongEncodeTime.updateAverage(time);
    }
    _averageEncodeTime.updateAverage(time); 
}

void OctreeServer::trackTreeWaitTime(float time) { 
    const float MAX_SHORT_TIME = 10.0f;
    const float MAX_LONG_TIME = 100.0f;
    if (time == SKIP_TIME) {
        _noTreeWait++;
        time = 0.0f;
    } else if (time <= MAX_SHORT_TIME) {
        _shortTreeWait++;
        _averageTreeShortWaitTime.updateAverage(time);
    } else if (time <= MAX_LONG_TIME) {
        _longTreeWait++;
        _averageTreeLongWaitTime.updateAverage(time);
    } else {
        _extraLongTreeWait++;
        _averageTreeExtraLongWaitTime.updateAverage(time);
    }
    _averageTreeWaitTime.updateAverage(time);
}

void OctreeServer::trackCompressAndWriteTime(float time) { 
    const float MAX_SHORT_TIME = 10.0f;
    const float MAX_LONG_TIME = 100.0f;
    if (time == SKIP_TIME) {
        _noCompress++;
        time = 0.0f;
    } else if (time <= MAX_SHORT_TIME) {
        _shortCompress++;
        _averageShortCompressTime.updateAverage(time);
    } else if (time <= MAX_LONG_TIME) {
        _longCompress++;
        _averageLongCompressTime.updateAverage(time);
    } else {
        _extraLongCompress++;
        _averageExtraLongCompressTime.updateAverage(time);
    }
    _averageCompressAndWriteTime.updateAverage(time); 
}

void OctreeServer::trackPacketSendingTime(float time) { 
    if (time == SKIP_TIME) {
        _noSend++;
        time = 0.0f;
    }
    _averagePacketSendingTime.updateAverage(time); 
}


void OctreeServer::trackProcessWaitTime(float time) { 
    const float MAX_SHORT_TIME = 10.0f;
    const float MAX_LONG_TIME = 100.0f;
    if (time == SKIP_TIME) {
        _noProcessWait++;
        time = 0.0f;
    } else if (time <= MAX_SHORT_TIME) {
        _shortProcessWait++;
        _averageProcessShortWaitTime.updateAverage(time);
    } else if (time <= MAX_LONG_TIME) {
        _longProcessWait++;
        _averageProcessLongWaitTime.updateAverage(time);
    } else {
        _extraLongProcessWait++;
        _averageProcessExtraLongWaitTime.updateAverage(time);
    }
    _averageProcessWaitTime.updateAverage(time);
}

void OctreeServer::attachQueryNodeToNode(Node* newNode) {
    if (!newNode->getLinkedData() && _instance) {
        OctreeQueryNode* newQueryNodeData = _instance->createOctreeQueryNode();
        newQueryNodeData->init();
        newNode->setLinkedData(newQueryNodeData);
    }
}

OctreeServer::OctreeServer(const QByteArray& packet) :
    ThreadedAssignment(packet),
    _argc(0),
    _argv(NULL),
    _parsedArgV(NULL),
    _httpManager(NULL),
    _statusPort(0),
    _packetsPerClientPerInterval(10),
    _packetsTotalPerInterval(DEFAULT_PACKETS_PER_INTERVAL),
    _tree(NULL),
    _wantPersist(true),
    _debugSending(false),
    _debugReceiving(false),
    _verboseDebug(false),
    _jurisdiction(NULL),
    _jurisdictionSender(NULL),
    _octreeInboundPacketProcessor(NULL),
    _persistThread(NULL),
    _started(time(0)),
    _startedUSecs(usecTimestampNow())
{
    if (_instance) {
        qDebug() << "Octree Server starting... while old instance still running _instance=["<<_instance<<"] this=[" << this << "]";
    }

    qDebug() << "Octree Server starting... setting _instance to=[" << this << "]";
    _instance = this;

    _averageLoopTime.updateAverage(0);
    qDebug() << "Octree server starting... [" << this << "]";
    
    // make sure the AccountManager has an Auth URL for payment redemptions
    
    AccountManager::getInstance().setAuthURL(DEFAULT_NODE_AUTH_URL);
}

OctreeServer::~OctreeServer() {
    qDebug() << qPrintable(_safeServerName) << "server shutting down... [" << this << "]";
    if (_parsedArgV) {
        for (int i = 0; i < _argc; i++) {
            delete[] _parsedArgV[i];
        }
        delete[] _parsedArgV;
    }

    if (_jurisdictionSender) {
        _jurisdictionSender->terminate();
        _jurisdictionSender->deleteLater();
    }

    if (_octreeInboundPacketProcessor) {
        _octreeInboundPacketProcessor->terminate();
        _octreeInboundPacketProcessor->deleteLater();
    }

    if (_persistThread) {
        _persistThread->terminate();
        _persistThread->deleteLater();
    }

    delete _jurisdiction;
    _jurisdiction = NULL;
    
    // cleanup our tree here...
    qDebug() << qPrintable(_safeServerName) << "server START cleaning up octree... [" << this << "]";
    delete _tree;
    _tree = NULL;
    qDebug() << qPrintable(_safeServerName) << "server DONE cleaning up octree... [" << this << "]";
    
    if (_instance == this) {
        _instance = NULL; // we are gone
    }
    qDebug() << qPrintable(_safeServerName) << "server DONE shutting down... [" << this << "]";
}

void OctreeServer::initHTTPManager(int port) {
    // setup the embedded web server

    QString documentRoot = QString("%1/resources/web").arg(QCoreApplication::applicationDirPath());

    // setup an httpManager with us as the request handler and the parent
    _httpManager = new HTTPManager(port, documentRoot, this, this);
}

bool OctreeServer::handleHTTPRequest(HTTPConnection* connection, const QUrl& url) {

#ifdef FORCE_CRASH
    if (connection->requestOperation() == QNetworkAccessManager::GetOperation
        && path == "/force_crash") {

        qDebug() << "About to force a crash!";

        int foo;
        int* forceCrash = &foo;

        QString responseString("forcing a crash...");
        connection->respond(HTTPConnection::StatusCode200, qPrintable(responseString));

        delete[] forceCrash;

        return true;
    }
#endif

    bool showStats = false;

    if (connection->requestOperation() == QNetworkAccessManager::GetOperation) {
        if (url.path() == "/") {
            showStats = true;
        } else if (url.path() == "/resetStats") {
            _octreeInboundPacketProcessor->resetStats();
            resetSendingStats();
            showStats = true;
        }
    }

    if (showStats) {
        quint64 checkSum;
        // return a 200
        QString statsString("<html><doc>\r\n<pre>\r\n");
        statsString += QString("<b>Your %1 Server is running... <a href='/'>[RELOAD]</a></b>\r\n").arg(getMyServerName());

        tm* localtm = localtime(&_started);
        const int MAX_TIME_LENGTH = 128;
        char buffer[MAX_TIME_LENGTH];
        strftime(buffer, MAX_TIME_LENGTH, "%m/%d/%Y %X", localtm);
        statsString += QString("Running since: %1").arg(buffer);

        // Convert now to tm struct for UTC
        tm* gmtm = gmtime(&_started);
        if (gmtm) {
            strftime(buffer, MAX_TIME_LENGTH, "%m/%d/%Y %X", gmtm);
            statsString += (QString(" [%1 UTM] ").arg(buffer));
        }

        statsString += "\r\n";

        statsString += "Uptime: " + getUptime();
        statsString += "\r\n\r\n";

        // display voxel file load time
        if (isInitialLoadComplete()) {
            if (isPersistEnabled()) {
                statsString += QString("%1 File Persist Enabled...\r\n").arg(getMyServerName());
            } else {
                statsString += QString("%1 File Persist Disabled...\r\n").arg(getMyServerName());
            }

            statsString += "\r\n";

            statsString += QString("%1 File Load Took ").arg(getMyServerName());
            statsString += getFileLoadTime();
            statsString += "\r\n";

        } else {
            statsString += "Voxels not yet loaded...\r\n";
        }

        statsString += "\r\n\r\n";
        statsString += "<b>Configuration:</b>\r\n";
        statsString += getConfiguration() + "\r\n"; //one to end the config line
        statsString += "\r\n";

        const int COLUMN_WIDTH = 19;
        QLocale locale(QLocale::English);
        const float AS_PERCENT = 100.0;

        statsString += QString("        Configured Max PPS/Client: %1 pps/client\r\n")
            .arg(locale.toString((uint)getPacketsPerClientPerSecond()).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("        Configured Max PPS/Server: %1 pps/server\r\n\r\n")
            .arg(locale.toString((uint)getPacketsTotalPerSecond()).rightJustified(COLUMN_WIDTH, ' '));


        // display scene stats
        unsigned long nodeCount = OctreeElement::getNodeCount();
        unsigned long internalNodeCount = OctreeElement::getInternalNodeCount();
        unsigned long leafNodeCount = OctreeElement::getLeafNodeCount();

        statsString += "<b>Current Elements in scene:</b>\r\n";
        statsString += QString("       Total Elements: %1 nodes\r\n")
            .arg(locale.toString((uint)nodeCount).rightJustified(16, ' '));
        statsString += QString().sprintf("    Internal Elements: %s nodes (%5.2f%%)\r\n",
                                         locale.toString((uint)internalNodeCount).rightJustified(16,
                                                                                                 ' ').toLocal8Bit().constData(),
                                         ((float)internalNodeCount / (float)nodeCount) * AS_PERCENT);
        statsString += QString().sprintf("        Leaf Elements: %s nodes (%5.2f%%)\r\n",
                                         locale.toString((uint)leafNodeCount).rightJustified(16, ' ').toLocal8Bit().constData(),
                                         ((float)leafNodeCount / (float)nodeCount) * AS_PERCENT);
        statsString += "\r\n";
        statsString += "\r\n";

        // display outbound packet stats
        statsString += QString("<b>%1 Outbound Packet Statistics... "
                                "<a href='/resetStats'>[RESET]</a></b>\r\n").arg(getMyServerName());

        quint64 totalOutboundPackets = OctreeSendThread::_totalPackets;
        quint64 totalOutboundBytes = OctreeSendThread::_totalBytes;
        quint64 totalWastedBytes = OctreeSendThread::_totalWastedBytes;
        quint64 totalBytesOfOctalCodes = OctreePacketData::getTotalBytesOfOctalCodes();
        quint64 totalBytesOfBitMasks = OctreePacketData::getTotalBytesOfBitMasks();
        quint64 totalBytesOfColor = OctreePacketData::getTotalBytesOfColor();

        statsString += QString("          Total Clients Connected: %1 clients\r\n")
            .arg(locale.toString((uint)getCurrentClientCount()).rightJustified(COLUMN_WIDTH, ' '));

        quint64 oneSecondAgo = usecTimestampNow() - USECS_PER_SECOND;
        
        statsString += QString("            process() last second: %1 clients\r\n")
            .arg(locale.toString((uint)howManyThreadsDidProcess(oneSecondAgo)).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("  packetDistributor() last second: %1 clients\r\n")
            .arg(locale.toString((uint)howManyThreadsDidPacketDistributor(oneSecondAgo)).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("   handlePacketSend() last second: %1 clients\r\n")
            .arg(locale.toString((uint)howManyThreadsDidHandlePacketSend(oneSecondAgo)).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("      writeDatagram() last second: %1 clients\r\n\r\n")
            .arg(locale.toString((uint)howManyThreadsDidCallWriteDatagram(oneSecondAgo)).rightJustified(COLUMN_WIDTH, ' '));

        float averageLoopTime = getAverageLoopTime();
        statsString += QString().sprintf("           Average packetLoop() time:      %7.2f msecs"
                                         "                 samples: %12d \r\n", 
                                         averageLoopTime, _averageLoopTime.getSampleCount());

        float averageInsideTime = getAverageInsideTime();
        statsString += QString().sprintf("               Average 'inside' time:    %9.2f usecs"
                                         "                 samples: %12d \r\n\r\n", 
                                         averageInsideTime, _averageInsideTime.getSampleCount());


        // Process Wait
        {
            int allWaitTimes = _extraLongProcessWait +_longProcessWait + _shortProcessWait + _noProcessWait;

            float averageProcessWaitTime = getAverageProcessWaitTime();
            statsString += QString().sprintf("      Average process lock wait time:"
                                             "    %9.2f usecs                 samples: %12d \r\n",
                                             averageProcessWaitTime, allWaitTimes);

            float zeroVsTotal = (allWaitTimes > 0) ? ((float)_noProcessWait / (float)allWaitTimes) : 0.0f;
            statsString += QString().sprintf("                        No Lock Wait:"
                                             "                          (%6.2f%%) samples: %12d \r\n",
                                             zeroVsTotal * AS_PERCENT, _noProcessWait);

            float shortVsTotal = (allWaitTimes > 0) ? ((float)_shortProcessWait / (float)allWaitTimes) : 0.0f;
            statsString += QString().sprintf("    Avg process lock short wait time:"
                                             "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                             _averageProcessShortWaitTime.getAverage(), 
                                             shortVsTotal * AS_PERCENT, _shortProcessWait);

            float longVsTotal = (allWaitTimes > 0) ? ((float)_longProcessWait / (float)allWaitTimes) : 0.0f;
            statsString += QString().sprintf("     Avg process lock long wait time:"
                                             "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                             _averageProcessLongWaitTime.getAverage(), 
                                             longVsTotal * AS_PERCENT, _longProcessWait);

            float extraLongVsTotal = (allWaitTimes > 0) ? ((float)_extraLongProcessWait / (float)allWaitTimes) : 0.0f;
            statsString += QString().sprintf("Avg process lock extralong wait time:"
                                             "          %9.2f usecs (%6.2f%%) samples: %12d \r\n\r\n",
                                             _averageProcessExtraLongWaitTime.getAverage(), 
                                             extraLongVsTotal * AS_PERCENT, _extraLongProcessWait);
        }

        // Tree Wait
        int allWaitTimes = _extraLongTreeWait +_longTreeWait + _shortTreeWait + _noTreeWait;

        float averageTreeWaitTime = getAverageTreeWaitTime();
        statsString += QString().sprintf("         Average tree lock wait time:"
                                         "    %9.2f usecs                 samples: %12d \r\n",
                                         averageTreeWaitTime, allWaitTimes);

        float zeroVsTotal = (allWaitTimes > 0) ? ((float)_noTreeWait / (float)allWaitTimes) : 0.0f;
        statsString += QString().sprintf("                        No Lock Wait:"
                                         "                          (%6.2f%%) samples: %12d \r\n",
                                         zeroVsTotal * AS_PERCENT, _noTreeWait);

        float shortVsTotal = (allWaitTimes > 0) ? ((float)_shortTreeWait / (float)allWaitTimes) : 0.0f;
        statsString += QString().sprintf("       Avg tree lock short wait time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageTreeShortWaitTime.getAverage(), 
                                         shortVsTotal * AS_PERCENT, _shortTreeWait);

        float longVsTotal = (allWaitTimes > 0) ? ((float)_longTreeWait / (float)allWaitTimes) : 0.0f;
        statsString += QString().sprintf("        Avg tree lock long wait time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageTreeLongWaitTime.getAverage(), 
                                         longVsTotal * AS_PERCENT, _longTreeWait);

        float extraLongVsTotal = (allWaitTimes > 0) ? ((float)_extraLongTreeWait / (float)allWaitTimes) : 0.0f;
        statsString += QString().sprintf("  Avg tree lock extra long wait time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n\r\n",
                                         _averageTreeExtraLongWaitTime.getAverage(), 
                                         extraLongVsTotal * AS_PERCENT, _extraLongTreeWait);

        // encode
        float averageEncodeTime = getAverageEncodeTime();
        statsString += QString().sprintf("                 Average encode time:    %9.2f usecs\r\n", averageEncodeTime);
        
        int allEncodeTimes = _noEncode + _shortEncode + _longEncode + _extraLongEncode;

        float zeroVsTotalEncode = (allEncodeTimes > 0) ? ((float)_noEncode / (float)allEncodeTimes) : 0.0f;
        statsString += QString().sprintf("                           No Encode:"
                                         "                          (%6.2f%%) samples: %12d \r\n",
                                         zeroVsTotalEncode * AS_PERCENT, _noEncode);

        float shortVsTotalEncode = (allEncodeTimes > 0) ? ((float)_shortEncode / (float)allEncodeTimes) : 0.0f;
        statsString += QString().sprintf("               Avg short encode time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageShortEncodeTime.getAverage(), 
                                         shortVsTotalEncode * AS_PERCENT, _shortEncode);

        float longVsTotalEncode = (allEncodeTimes > 0) ? ((float)_longEncode / (float)allEncodeTimes) : 0.0f;
        statsString += QString().sprintf("                Avg long encode time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageLongEncodeTime.getAverage(), 
                                         longVsTotalEncode * AS_PERCENT, _longEncode);

        float extraLongVsTotalEncode = (allEncodeTimes > 0) ? ((float)_extraLongEncode / (float)allEncodeTimes) : 0.0f;
        statsString += QString().sprintf("          Avg extra long encode time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n\r\n",
                                         _averageExtraLongEncodeTime.getAverage(), 
                                         extraLongVsTotalEncode * AS_PERCENT, _extraLongEncode);


        float averageCompressAndWriteTime = getAverageCompressAndWriteTime();
        statsString += QString().sprintf("     Average compress and write time:    %9.2f usecs\r\n", 
            averageCompressAndWriteTime);

        int allCompressTimes = _noCompress + _shortCompress + _longCompress + _extraLongCompress;

        float zeroVsTotalCompress = (allCompressTimes > 0) ? ((float)_noCompress / (float)allCompressTimes) : 0.0f;
        statsString += QString().sprintf("                      No compression:"
                                         "                          (%6.2f%%) samples: %12d \r\n",
                                         zeroVsTotalCompress * AS_PERCENT, _noCompress);

        float shortVsTotalCompress = (allCompressTimes > 0) ? ((float)_shortCompress / (float)allCompressTimes) : 0.0f;
        statsString += QString().sprintf("             Avg short compress time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageShortCompressTime.getAverage(), 
                                         shortVsTotalCompress * AS_PERCENT, _shortCompress);

        float longVsTotalCompress = (allCompressTimes > 0) ? ((float)_longCompress / (float)allCompressTimes) : 0.0f;
        statsString += QString().sprintf("              Avg long compress time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n",
                                         _averageLongCompressTime.getAverage(), 
                                         longVsTotalCompress * AS_PERCENT, _longCompress);

        float extraLongVsTotalCompress = (allCompressTimes > 0) ? ((float)_extraLongCompress / (float)allCompressTimes) : 0.0f;
        statsString += QString().sprintf("        Avg extra long compress time:"
                                         "          %9.2f usecs (%6.2f%%) samples: %12d \r\n\r\n",
                                         _averageExtraLongCompressTime.getAverage(), 
                                         extraLongVsTotalCompress * AS_PERCENT, _extraLongCompress);

        float averagePacketSendingTime = getAveragePacketSendingTime();
        statsString += QString().sprintf("         Average packet sending time:    %9.2f usecs (includes node lock)\r\n", 
                                        averagePacketSendingTime);

        float noVsTotalSend = (_averagePacketSendingTime.getSampleCount() > 0) ? 
                                        ((float)_noSend / (float)_averagePacketSendingTime.getSampleCount()) : 0.0f;
        statsString += QString().sprintf("                         Not sending:"
                                         "                          (%6.2f%%) samples: %12d \r\n",
                                         noVsTotalSend * AS_PERCENT, _noSend);
                                        
        float averageNodeWaitTime = getAverageNodeWaitTime();
        statsString += QString().sprintf("         Average node lock wait time:    %9.2f usecs\r\n", averageNodeWaitTime);

        statsString += QString().sprintf("--------------------------------------------------------------\r\n");

        float encodeToInsidePercent = averageInsideTime == 0.0f ? 0.0f : (averageEncodeTime / averageInsideTime) * AS_PERCENT;
        statsString += QString().sprintf("                          encode ratio:      %5.2f%%\r\n", 
                                        encodeToInsidePercent);

        float waitToInsidePercent = averageInsideTime == 0.0f ? 0.0f 
                    : ((averageTreeWaitTime + averageNodeWaitTime) / averageInsideTime) * AS_PERCENT;
        statsString += QString().sprintf("                         waiting ratio:      %5.2f%%\r\n", waitToInsidePercent);

        float compressAndWriteToInsidePercent = averageInsideTime == 0.0f ? 0.0f 
                    : (averageCompressAndWriteTime / averageInsideTime) * AS_PERCENT;
        statsString += QString().sprintf("              compress and write ratio:      %5.2f%%\r\n", 
                    compressAndWriteToInsidePercent);

        float sendingToInsidePercent = averageInsideTime == 0.0f ? 0.0f 
                    : (averagePacketSendingTime / averageInsideTime) * AS_PERCENT;
        statsString += QString().sprintf("                         sending ratio:      %5.2f%%\r\n", sendingToInsidePercent);
        


        statsString += QString("\r\n");

        statsString += QString("           Total Outbound Packets: %1 packets\r\n")
            .arg(locale.toString((uint)totalOutboundPackets).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("             Total Outbound Bytes: %1 bytes\r\n")
            .arg(locale.toString((uint)totalOutboundBytes).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("               Total Wasted Bytes: %1 bytes\r\n")
            .arg(locale.toString((uint)totalWastedBytes).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString().sprintf("            Total OctalCode Bytes: %s bytes (%5.2f%%)\r\n",
            locale.toString((uint)totalBytesOfOctalCodes).rightJustified(COLUMN_WIDTH, ' ').toLocal8Bit().constData(),
            ((float)totalBytesOfOctalCodes / (float)totalOutboundBytes) * AS_PERCENT);
        statsString += QString().sprintf("             Total BitMasks Bytes: %s bytes (%5.2f%%)\r\n",
            locale.toString((uint)totalBytesOfBitMasks).rightJustified(COLUMN_WIDTH, ' ').toLocal8Bit().constData(),
            ((float)totalBytesOfBitMasks / (float)totalOutboundBytes) * AS_PERCENT);
        statsString += QString().sprintf("                Total Color Bytes: %s bytes (%5.2f%%)\r\n",
            locale.toString((uint)totalBytesOfColor).rightJustified(COLUMN_WIDTH, ' ').toLocal8Bit().constData(),
            ((float)totalBytesOfColor / (float)totalOutboundBytes) * AS_PERCENT);

        statsString += "\r\n";
        statsString += "\r\n";

        // display inbound packet stats
        statsString += QString().sprintf("<b>%s Edit Statistics... <a href='/resetStats'>[RESET]</a></b>\r\n",
                                         getMyServerName());
        quint64 averageTransitTimePerPacket = _octreeInboundPacketProcessor->getAverageTransitTimePerPacket();
        quint64 averageProcessTimePerPacket = _octreeInboundPacketProcessor->getAverageProcessTimePerPacket();
        quint64 averageLockWaitTimePerPacket = _octreeInboundPacketProcessor->getAverageLockWaitTimePerPacket();
        quint64 averageProcessTimePerElement = _octreeInboundPacketProcessor->getAverageProcessTimePerElement();
        quint64 averageLockWaitTimePerElement = _octreeInboundPacketProcessor->getAverageLockWaitTimePerElement();
        quint64 totalElementsProcessed = _octreeInboundPacketProcessor->getTotalElementsProcessed();
        quint64 totalPacketsProcessed = _octreeInboundPacketProcessor->getTotalPacketsProcessed();

        float averageElementsPerPacket = totalPacketsProcessed == 0 ? 0 : totalElementsProcessed / totalPacketsProcessed;

        statsString += QString("           Total Inbound Packets: %1 packets\r\n")
            .arg(locale.toString((uint)totalPacketsProcessed).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("          Total Inbound Elements: %1 elements\r\n")
            .arg(locale.toString((uint)totalElementsProcessed).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString().sprintf(" Average Inbound Elements/Packet: %f elements/packet\r\n", averageElementsPerPacket);
        statsString += QString("     Average Transit Time/Packet: %1 usecs\r\n")
            .arg(locale.toString((uint)averageTransitTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("     Average Process Time/Packet: %1 usecs\r\n")
            .arg(locale.toString((uint)averageProcessTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("   Average Wait Lock Time/Packet: %1 usecs\r\n")
            .arg(locale.toString((uint)averageLockWaitTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("    Average Process Time/Element: %1 usecs\r\n")
            .arg(locale.toString((uint)averageProcessTimePerElement).rightJustified(COLUMN_WIDTH, ' '));
        statsString += QString("  Average Wait Lock Time/Element: %1 usecs\r\n")
            .arg(locale.toString((uint)averageLockWaitTimePerElement).rightJustified(COLUMN_WIDTH, ' '));


        int senderNumber = 0;
        NodeToSenderStatsMap& allSenderStats = _octreeInboundPacketProcessor->getSingleSenderStats();
        for (NodeToSenderStatsMapConstIterator i = allSenderStats.begin(); i != allSenderStats.end(); i++) {
            senderNumber++;
            QUuid senderID = i.key();
            const SingleSenderStats& senderStats = i.value();

            statsString += QString("\r\n             Stats for sender %1 uuid: %2\r\n")
                .arg(senderNumber).arg(senderID.toString());

            averageTransitTimePerPacket = senderStats.getAverageTransitTimePerPacket();
            averageProcessTimePerPacket = senderStats.getAverageProcessTimePerPacket();
            averageLockWaitTimePerPacket = senderStats.getAverageLockWaitTimePerPacket();
            averageProcessTimePerElement = senderStats.getAverageProcessTimePerElement();
            averageLockWaitTimePerElement = senderStats.getAverageLockWaitTimePerElement();
            totalElementsProcessed = senderStats.getTotalElementsProcessed();
            totalPacketsProcessed = senderStats.getTotalPacketsProcessed();

            averageElementsPerPacket = totalPacketsProcessed == 0 ? 0 : totalElementsProcessed / totalPacketsProcessed;

            statsString += QString("               Total Inbound Packets: %1 packets\r\n")
                .arg(locale.toString((uint)totalPacketsProcessed).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString("              Total Inbound Elements: %1 elements\r\n")
                .arg(locale.toString((uint)totalElementsProcessed).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString().sprintf("     Average Inbound Elements/Packet: %f elements/packet\r\n",
                                             averageElementsPerPacket);
            statsString += QString("         Average Transit Time/Packet: %1 usecs\r\n")
                .arg(locale.toString((uint)averageTransitTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString("        Average Process Time/Packet: %1 usecs\r\n")
                .arg(locale.toString((uint)averageProcessTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString("       Average Wait Lock Time/Packet: %1 usecs\r\n")
                .arg(locale.toString((uint)averageLockWaitTimePerPacket).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString("        Average Process Time/Element: %1 usecs\r\n")
                .arg(locale.toString((uint)averageProcessTimePerElement).rightJustified(COLUMN_WIDTH, ' '));
            statsString += QString("      Average Wait Lock Time/Element: %1 usecs\r\n")
                .arg(locale.toString((uint)averageLockWaitTimePerElement).rightJustified(COLUMN_WIDTH, ' '));

        }

        statsString += "\r\n\r\n";

        // display memory usage stats
        statsString += "<b>Current Memory Usage Statistics</b>\r\n";
        statsString += QString().sprintf("\r\nOctreeElement size... %ld bytes\r\n", sizeof(OctreeElement));
        statsString += "\r\n";

        const char* memoryScaleLabel;
        const float MEGABYTES = 1000000.f;
        const float GIGABYTES = 1000000000.f;
        float memoryScale;
        if (OctreeElement::getTotalMemoryUsage() / MEGABYTES < 1000.0f) {
            memoryScaleLabel = "MB";
            memoryScale = MEGABYTES;
        } else {
            memoryScaleLabel = "GB";
            memoryScale = GIGABYTES;
        }

        statsString += QString().sprintf("Element Node Memory Usage:       %8.2f %s\r\n",
                                         OctreeElement::getVoxelMemoryUsage() / memoryScale, memoryScaleLabel);
        statsString += QString().sprintf("Octcode Memory Usage:            %8.2f %s\r\n",
                                         OctreeElement::getOctcodeMemoryUsage() / memoryScale, memoryScaleLabel);
        statsString += QString().sprintf("External Children Memory Usage:  %8.2f %s\r\n",
                                         OctreeElement::getExternalChildrenMemoryUsage() / memoryScale, memoryScaleLabel);
        statsString += "                                 -----------\r\n";
        statsString += QString().sprintf("                         Total:  %8.2f %s\r\n",
                                         OctreeElement::getTotalMemoryUsage() / memoryScale, memoryScaleLabel);
        statsString += "\r\n";

        statsString += "OctreeElement Children Population Statistics...\r\n";
        checkSum = 0;
        for (int i=0; i <= NUMBER_OF_CHILDREN; i++) {
            checkSum += OctreeElement::getChildrenCount(i);
            statsString += QString().sprintf("    Nodes with %d children:      %s nodes (%5.2f%%)\r\n", i,
                locale.toString((uint)OctreeElement::getChildrenCount(i)).rightJustified(16, ' ').toLocal8Bit().constData(),
                ((float)OctreeElement::getChildrenCount(i) / (float)nodeCount) * AS_PERCENT);
        }
        statsString += "                                ----------------------\r\n";
        statsString += QString("                    Total:      %1 nodes\r\n")
            .arg(locale.toString((uint)checkSum).rightJustified(16, ' '));

#ifdef BLENDED_UNION_CHILDREN
        statsString += "\r\n";
        statsString += "OctreeElement Children Encoding Statistics...\r\n";

        statsString += QString().sprintf("    Single or No Children:      %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getSingleChildrenCount(),
             ((float)OctreeElement::getSingleChildrenCount() / (float)nodeCount) * AS_PERCENT));
        statsString += QString().sprintf("    Two Children as Offset:     %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getTwoChildrenOffsetCount(),
             ((float)OctreeElement::getTwoChildrenOffsetCount() / (float)nodeCount) * AS_PERCENT));
        statsString += QString().sprintf("    Two Children as External:   %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getTwoChildrenExternalCount(),
             ((float)OctreeElement::getTwoChildrenExternalCount() / (float)nodeCount) * AS_PERCENT);
        statsString += QString().sprintf("    Three Children as Offset:   %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getThreeChildrenOffsetCount(),
             ((float)OctreeElement::getThreeChildrenOffsetCount() / (float)nodeCount) * AS_PERCENT);
        statsString += QString().sprintf("    Three Children as External: %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getThreeChildrenExternalCount(),
             ((float)OctreeElement::getThreeChildrenExternalCount() / (float)nodeCount) * AS_PERCENT);
        statsString += QString().sprintf("    Children as External Array: %10.llu nodes (%5.2f%%)\r\n",
             OctreeElement::getExternalChildrenCount(),
             ((float)OctreeElement::getExternalChildrenCount() / (float)nodeCount) * AS_PERCENT);

        checkSum = OctreeElement::getSingleChildrenCount() +
        OctreeElement::getTwoChildrenOffsetCount() + OctreeElement::getTwoChildrenExternalCount() +
        OctreeElement::getThreeChildrenOffsetCount() + OctreeElement::getThreeChildrenExternalCount() +
        OctreeElement::getExternalChildrenCount();

        statsString += "                                ----------------\r\n";
        statsString += QString().sprintf("                         Total: %10.llu nodes\r\n", checkSum);
        statsString += QString().sprintf("                      Expected: %10.lu nodes\r\n", nodeCount);

        statsString += "\r\n";
        statsString += "In other news....\r\n";

        statsString += QString().sprintf("could store 4 children internally:     %10.llu nodes\r\n",
                                         OctreeElement::getCouldStoreFourChildrenInternally());
        statsString += QString().sprintf("could NOT store 4 children internally: %10.llu nodes\r\n",
                                         OctreeElement::getCouldNotStoreFourChildrenInternally());
#endif

        statsString += "\r\n\r\n";
        statsString += "</pre>\r\n";
        statsString += "</doc></html>";

        connection->respond(HTTPConnection::StatusCode200, qPrintable(statsString), "text/html");

        return true;
    } else {
        // have HTTPManager attempt to process this request from the document_root
        return false;
    }

}

void OctreeServer::setArguments(int argc, char** argv) {
    _argc = argc;
    _argv = const_cast<const char**>(argv);

    qDebug("OctreeServer::setArguments()");
    for (int i = 0; i < _argc; i++) {
        qDebug("_argv[%d]=%s", i, _argv[i]);
    }

}

void OctreeServer::parsePayload() {

    if (getPayload().size() > 0) {
        QString config(_payload);

        // Now, parse the config
        QStringList configList = config.split(" ");

        int argCount = configList.size() + 1;

        qDebug("OctreeServer::parsePayload()... argCount=%d",argCount);

        _parsedArgV = new char*[argCount];
        const char* dummy = "config-from-payload";
        _parsedArgV[0] = new char[strlen(dummy) + sizeof(char)];
        strcpy(_parsedArgV[0], dummy);

        for (int i = 1; i < argCount; i++) {
            QString configItem = configList.at(i-1);
            _parsedArgV[i] = new char[configItem.length() + sizeof(char)];
            strcpy(_parsedArgV[i], configItem.toLocal8Bit().constData());
            qDebug("OctreeServer::parsePayload()... _parsedArgV[%d]=%s", i, _parsedArgV[i]);
        }

        setArguments(argCount, _parsedArgV);
    }
}

void OctreeServer::readPendingDatagrams() {
    QByteArray receivedPacket;
    HifiSockAddr senderSockAddr;
    
    NodeList* nodeList = NodeList::getInstance();
    
    while (readAvailableDatagram(receivedPacket, senderSockAddr)) {
        if (nodeList->packetVersionAndHashMatch(receivedPacket)) {
            PacketType packetType = packetTypeForPacket(receivedPacket);
            SharedNodePointer matchingNode = nodeList->sendingNodeForPacket(receivedPacket);
            if (packetType == getMyQueryMessageType()) {
                // If we got a query packet, then we're talking to an agent, and we
                // need to make sure we have it in our nodeList.
                if (matchingNode) {
                    nodeList->updateNodeWithDataFromPacket(matchingNode, receivedPacket);
                    OctreeQueryNode* nodeData = (OctreeQueryNode*)matchingNode->getLinkedData();
                    if (nodeData && !nodeData->isOctreeSendThreadInitalized()) {
                        
                        // NOTE: this is an important aspect of the proper ref counting. The send threads/node data need to 
                        // know that the OctreeServer/Assignment will not get deleted on it while it's still active. The 
                        // solution is to get the shared pointer for the current assignment. We need to make sure this is the 
                        // same SharedAssignmentPointer that was ref counted by the assignment client.                    
                        SharedAssignmentPointer sharedAssignment = AssignmentClient::getCurrentAssignment();
                        nodeData->initializeOctreeSendThread(sharedAssignment, matchingNode);
                    }
                }
            } else if (packetType == PacketTypeOctreeDataNack) {
                // If we got a nack packet, then we're talking to an agent, and we
                // need to make sure we have it in our nodeList.
                if (matchingNode) {
                    OctreeQueryNode* nodeData = (OctreeQueryNode*)matchingNode->getLinkedData();
                    if (nodeData) {
                        nodeData->parseNackPacket(receivedPacket);
                    }
                }
            } else if (packetType == PacketTypeJurisdictionRequest) {
                _jurisdictionSender->queueReceivedPacket(matchingNode, receivedPacket);
            } else if (packetType == PacketTypeSignedTransactionPayment) {
                handleSignedTransactionPayment(packetType, receivedPacket);
            } else if (_octreeInboundPacketProcessor && getOctree()->handlesEditPacketType(packetType)) {
                _octreeInboundPacketProcessor->queueReceivedPacket(matchingNode, receivedPacket);
            } else {
                // let processNodeData handle it.
                NodeList::getInstance()->processNodeData(senderSockAddr, receivedPacket);
            }
        }
    }
}

void OctreeServer::run() {
    _safeServerName = getMyServerName();
    
    // Before we do anything else, create our tree...
    OctreeElement::resetPopulationStatistics();
    _tree = createTree();
    
    // use common init to setup common timers and logging
    commonInit(getMyLoggingServerTargetName(), getMyNodeType());

    // Now would be a good time to parse our arguments, if we got them as assignment
    if (getPayload().size() > 0) {
        parsePayload();
    }

    beforeRun(); // after payload has been processed

    qInstallMessageHandler(Logging::verboseMessageHandler);

    const char* STATUS_PORT = "--statusPort";
    const char* statusPort = getCmdOption(_argc, _argv, STATUS_PORT);
    if (statusPort) {
        _statusPort = atoi(statusPort);
        initHTTPManager(_statusPort);
    }
    
    const char* STATUS_HOST = "--statusHost";
    const char* statusHost = getCmdOption(_argc, _argv, STATUS_HOST);
    if (statusHost) {
        qDebug("--statusHost=%s", statusHost);
        _statusHost = statusHost;
    } else {
        _statusHost = QHostAddress(getHostOrderLocalAddress()).toString();
    }
    qDebug("statusHost=%s", qPrintable(_statusHost));


    const char* JURISDICTION_FILE = "--jurisdictionFile";
    const char* jurisdictionFile = getCmdOption(_argc, _argv, JURISDICTION_FILE);
    if (jurisdictionFile) {
        qDebug("jurisdictionFile=%s", jurisdictionFile);

        qDebug("about to readFromFile().... jurisdictionFile=%s", jurisdictionFile);
        _jurisdiction = new JurisdictionMap(jurisdictionFile);
        qDebug("after readFromFile().... jurisdictionFile=%s", jurisdictionFile);
    } else {
        const char* JURISDICTION_ROOT = "--jurisdictionRoot";
        const char* jurisdictionRoot = getCmdOption(_argc, _argv, JURISDICTION_ROOT);
        if (jurisdictionRoot) {
            qDebug("jurisdictionRoot=%s", jurisdictionRoot);
        }

        const char* JURISDICTION_ENDNODES = "--jurisdictionEndNodes";
        const char* jurisdictionEndNodes = getCmdOption(_argc, _argv, JURISDICTION_ENDNODES);
        if (jurisdictionEndNodes) {
            qDebug("jurisdictionEndNodes=%s", jurisdictionEndNodes);
        }

        if (jurisdictionRoot || jurisdictionEndNodes) {
            _jurisdiction = new JurisdictionMap(jurisdictionRoot, jurisdictionEndNodes);
        }
    }

    NodeList* nodeList = NodeList::getInstance();
    nodeList->setOwnerType(getMyNodeType());

    connect(nodeList, SIGNAL(nodeAdded(SharedNodePointer)), SLOT(nodeAdded(SharedNodePointer)));
    connect(nodeList, SIGNAL(nodeKilled(SharedNodePointer)),SLOT(nodeKilled(SharedNodePointer)));


    // we need to ask the DS about agents so we can ping/reply with them
    nodeList->addNodeTypeToInterestSet(NodeType::Agent);

#ifndef WIN32
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif

    nodeList->linkedDataCreateCallback = &OctreeServer::attachQueryNodeToNode;

    srand((unsigned)time(0));

    const char* VERBOSE_DEBUG = "--verboseDebug";
    _verboseDebug =  cmdOptionExists(_argc, _argv, VERBOSE_DEBUG);
    qDebug("verboseDebug=%s", debug::valueOf(_verboseDebug));

    const char* DEBUG_SENDING = "--debugSending";
    _debugSending =  cmdOptionExists(_argc, _argv, DEBUG_SENDING);
    qDebug("debugSending=%s", debug::valueOf(_debugSending));

    const char* DEBUG_RECEIVING = "--debugReceiving";
    _debugReceiving =  cmdOptionExists(_argc, _argv, DEBUG_RECEIVING);
    qDebug("debugReceiving=%s", debug::valueOf(_debugReceiving));

    // By default we will persist, if you want to disable this, then pass in this parameter
    const char* NO_PERSIST = "--NoPersist";
    if (cmdOptionExists(_argc, _argv, NO_PERSIST)) {
        _wantPersist = false;
    }
    qDebug("wantPersist=%s", debug::valueOf(_wantPersist));

    // if we want Persistence, set up the local file and persist thread
    if (_wantPersist) {

        // Check to see if the user passed in a command line option for setting packet send rate
        const char* PERSIST_FILENAME = "--persistFilename";
        const char* persistFilenameParameter = getCmdOption(_argc, _argv, PERSIST_FILENAME);
        if (persistFilenameParameter) {
            strcpy(_persistFilename, persistFilenameParameter);
        } else {
            strcpy(_persistFilename, getMyDefaultPersistFilename());
        }

        qDebug("persistFilename=%s", _persistFilename);

        // now set up PersistThread
        _persistThread = new OctreePersistThread(_tree, _persistFilename);
        if (_persistThread) {
            _persistThread->initialize(true);
        }
    }

    // Debug option to demonstrate that the server's local time does not
    // need to be in sync with any other network node. This forces clock
    // skew for the individual server node
    const char* CLOCK_SKEW = "--clockSkew";
    const char* clockSkewOption = getCmdOption(_argc, _argv, CLOCK_SKEW);
    if (clockSkewOption) {
        int clockSkew = atoi(clockSkewOption);
        usecTimestampNowForceClockSkew(clockSkew);
        qDebug("clockSkewOption=%s clockSkew=%d", clockSkewOption, clockSkew);
    }

    // Check to see if the user passed in a command line option for setting packet send rate
    const char* PACKETS_PER_SECOND_PER_CLIENT_MAX = "--packetsPerSecondPerClientMax";
    const char* packetsPerSecondPerClientMax = getCmdOption(_argc, _argv, PACKETS_PER_SECOND_PER_CLIENT_MAX);
    if (packetsPerSecondPerClientMax) {
        _packetsPerClientPerInterval = atoi(packetsPerSecondPerClientMax) / INTERVALS_PER_SECOND;
        if (_packetsPerClientPerInterval < 1) {
            _packetsPerClientPerInterval = 1;
        }
    }
    qDebug("packetsPerSecondPerClientMax=%s _packetsPerClientPerInterval=%d", 
                    packetsPerSecondPerClientMax, _packetsPerClientPerInterval);

    // Check to see if the user passed in a command line option for setting packet send rate
    const char* PACKETS_PER_SECOND_TOTAL_MAX = "--packetsPerSecondTotalMax";
    const char* packetsPerSecondTotalMax = getCmdOption(_argc, _argv, PACKETS_PER_SECOND_TOTAL_MAX);
    if (packetsPerSecondTotalMax) {
        _packetsTotalPerInterval = atoi(packetsPerSecondTotalMax) / INTERVALS_PER_SECOND;
        if (_packetsTotalPerInterval < 1) {
            _packetsTotalPerInterval = 1;
        }
    }
    qDebug("packetsPerSecondTotalMax=%s _packetsTotalPerInterval=%d", 
                    packetsPerSecondTotalMax, _packetsTotalPerInterval);

    HifiSockAddr senderSockAddr;

    // set up our jurisdiction broadcaster...
    if (_jurisdiction) {
        _jurisdiction->setNodeType(getMyNodeType());
    }
    _jurisdictionSender = new JurisdictionSender(_jurisdiction, getMyNodeType());
    _jurisdictionSender->initialize(true);

    // set up our OctreeServerPacketProcessor
    _octreeInboundPacketProcessor = new OctreeInboundPacketProcessor(this);
    _octreeInboundPacketProcessor->initialize(true);

    // Convert now to tm struct for local timezone
    tm* localtm = localtime(&_started);
    const int MAX_TIME_LENGTH = 128;
    char localBuffer[MAX_TIME_LENGTH] = { 0 };
    char utcBuffer[MAX_TIME_LENGTH] = { 0 };
    strftime(localBuffer, MAX_TIME_LENGTH, "%m/%d/%Y %X", localtm);
    // Convert now to tm struct for UTC
    tm* gmtm = gmtime(&_started);
    if (gmtm) {
        strftime(utcBuffer, MAX_TIME_LENGTH, " [%m/%d/%Y %X UTC]", gmtm);
    }
    qDebug() << "Now running... started at: " << localBuffer << utcBuffer;
}

void OctreeServer::nodeAdded(SharedNodePointer node) {
    // we might choose to use this notifier to track clients in a pending state
    qDebug() << qPrintable(_safeServerName) << "server added node:" << *node;
}

void OctreeServer::nodeKilled(SharedNodePointer node) {
    quint64 start  = usecTimestampNow();

    // calling this here since nodeKilled slot in ReceivedPacketProcessor can't be triggered by signals yet!!
    _octreeInboundPacketProcessor->nodeKilled(node);

    qDebug() << qPrintable(_safeServerName) << "server killed node:" << *node;
    OctreeQueryNode* nodeData = static_cast<OctreeQueryNode*>(node->getLinkedData());
    if (nodeData) {
        nodeData->nodeKilled(); // tell our node data and sending threads that we'd like to shut down
    } else {
        qDebug() << qPrintable(_safeServerName) << "server node missing linked data node:" << *node;
    }

    quint64 end  = usecTimestampNow();
    quint64 usecsElapsed = (end - start);
    if (usecsElapsed > 1000) {
        qDebug() << qPrintable(_safeServerName) << "server nodeKilled() took: " << usecsElapsed << " usecs for node:" << *node;
    }
}

void OctreeServer::forceNodeShutdown(SharedNodePointer node) {
    quint64 start  = usecTimestampNow();

    qDebug() << qPrintable(_safeServerName) << "server killed node:" << *node;
    OctreeQueryNode* nodeData = static_cast<OctreeQueryNode*>(node->getLinkedData());
    if (nodeData) {
        nodeData->forceNodeShutdown(); // tell our node data and sending threads that we'd like to shut down
    } else {
        qDebug() << qPrintable(_safeServerName) << "server node missing linked data node:" << *node;
    }

    quint64 end  = usecTimestampNow();
    quint64 usecsElapsed = (end - start);
    qDebug() << qPrintable(_safeServerName) << "server forceNodeShutdown() took: "  
                << usecsElapsed << " usecs for node:" << *node;
}


void OctreeServer::aboutToFinish() {
    qDebug() << qPrintable(_safeServerName) << "server STARTING about to finish...";
    qDebug() << qPrintable(_safeServerName) << "inform Octree Inbound Packet Processor that we are shutting down...";
    _octreeInboundPacketProcessor->shuttingDown();
    foreach (const SharedNodePointer& node, NodeList::getInstance()->getNodeHash()) {
        qDebug() << qPrintable(_safeServerName) << "server about to finish while node still connected node:" << *node;
        forceNodeShutdown(node);
    }
    qDebug() << qPrintable(_safeServerName) << "server ENDING about to finish...";
}

QString OctreeServer::getUptime() {
    QString formattedUptime;
    quint64 now  = usecTimestampNow();
    const int USECS_PER_MSEC = 1000;
    quint64 msecsElapsed = (now - _startedUSecs) / USECS_PER_MSEC;
    const int MSECS_PER_SEC = 1000;
    const int SECS_PER_MIN = 60;
    const int MIN_PER_HOUR = 60;
    const int MSECS_PER_MIN = MSECS_PER_SEC * SECS_PER_MIN;

    float seconds = (msecsElapsed % MSECS_PER_MIN)/(float)MSECS_PER_SEC;
    int minutes = (msecsElapsed/(MSECS_PER_MIN)) % MIN_PER_HOUR;
    int hours = (msecsElapsed/(MSECS_PER_MIN * MIN_PER_HOUR));

    if (hours > 0) {
        formattedUptime += QString("%1 hour").arg(hours);
        if (hours > 1) {
            formattedUptime += QString("s");
        }
    }
    if (minutes > 0) {
        if (hours > 0) {
            formattedUptime += QString(" ");
        }
        formattedUptime += QString("%1 minute").arg(minutes);
        if (minutes > 1) {
            formattedUptime += QString("s");
        }
    }
    if (seconds > 0) {
        if (hours > 0 || minutes > 0) {
            formattedUptime += QString(" ");
        }
        formattedUptime += QString().sprintf("%.3f seconds", seconds);
    }
    return formattedUptime;
}

QString OctreeServer::getFileLoadTime() {
    QString result;
    if (isInitialLoadComplete()) {
        
        const int USECS_PER_MSEC = 1000;
        const int MSECS_PER_SEC = 1000;
        const int SECS_PER_MIN = 60;
        const int MIN_PER_HOUR = 60;
        const int MSECS_PER_MIN = MSECS_PER_SEC * SECS_PER_MIN;
        
        quint64 msecsElapsed = getLoadElapsedTime() / USECS_PER_MSEC;;
        float seconds = (msecsElapsed % MSECS_PER_MIN)/(float)MSECS_PER_SEC;
        int minutes = (msecsElapsed/(MSECS_PER_MIN)) % MIN_PER_HOUR;
        int hours = (msecsElapsed/(MSECS_PER_MIN * MIN_PER_HOUR));

        if (hours > 0) {
            result += QString("%1 hour").arg(hours);
            if (hours > 1) {
                result += QString("s");
            }
        }
        if (minutes > 0) {
            if (hours > 0) {
                result += QString(" ");
            }
            result += QString("%1 minute").arg(minutes);
            if (minutes > 1) {
                result += QString("s");
            }
        }
        if (seconds >= 0) {
            if (hours > 0 || minutes > 0) {
                result += QString(" ");
            }
            result += QString().sprintf("%.3f seconds", seconds);
        }
    } else {
        result = "Not yet loaded...";
    }
    return result;
}

QString OctreeServer::getConfiguration() {
    QString result;
    for (int i = 1; i < _argc; i++) {
        result += _argv[i] + QString(" ");
    }
    return result;
}

QString OctreeServer::getStatusLink() {
    QString result;
    if (_statusPort > 0) {
        QString detailedStats= QString("http://") +  _statusHost + QString(":%1").arg(_statusPort);
        result = "<a href='" + detailedStats + "'>"+detailedStats+"</a>";
    } else {
        result = "Status port not enabled.";
    }
    return result;
}

void OctreeServer::handleSignedTransactionPayment(PacketType packetType, const QByteArray& datagram) {
    // for now we're not verifying that this is actual payment for any octree edits
    // just use the AccountManager to send it up to the data server and have it redeemed
    AccountManager& accountManager = AccountManager::getInstance();
    
    const int NUM_BYTES_SIGNED_TRANSACTION_BINARY_MESSAGE = 72;
    const int NUM_BYTES_SIGNED_TRANSACTION_BINARY_SIGNATURE = 256;
    
    int numBytesPacketHeader = numBytesForPacketHeaderGivenPacketType(packetType);
    
    // pull out the transaction message in binary
    QByteArray messageHex = datagram.mid(numBytesPacketHeader, NUM_BYTES_SIGNED_TRANSACTION_BINARY_MESSAGE).toHex();
    // pull out the binary signed message digest
    QByteArray signatureHex = datagram.mid(numBytesPacketHeader + NUM_BYTES_SIGNED_TRANSACTION_BINARY_MESSAGE,
                                           NUM_BYTES_SIGNED_TRANSACTION_BINARY_SIGNATURE).toHex();
    
    // setup the QJSONObject we are posting
    QJsonObject postObject;
    
    const QString TRANSACTION_OBJECT_MESSAGE_KEY = "message";
    const QString TRANSACTION_OBJECT_SIGNATURE_KEY = "signature";
    const QString POST_OBJECT_TRANSACTION_KEY = "transaction";
    
    QJsonObject transactionObject;
    transactionObject.insert(TRANSACTION_OBJECT_MESSAGE_KEY, QString(messageHex));
    transactionObject.insert(TRANSACTION_OBJECT_SIGNATURE_KEY, QString(signatureHex));
    
    postObject.insert(POST_OBJECT_TRANSACTION_KEY, transactionObject);
    
    // setup our callback params
    JSONCallbackParameters callbackParameters;
    callbackParameters.jsonCallbackReceiver = this;
    callbackParameters.jsonCallbackMethod = "handleSignedTransactionPaymentResponse";
    
    accountManager.unauthenticatedRequest("/api/v1/transactions/redeem", QNetworkAccessManager::PostOperation,
                                          callbackParameters, QJsonDocument(postObject).toJson());
    
}

void OctreeServer::handleSignedTransactionPaymentResponse(const QJsonObject& jsonObject) {
    // pull the ID to debug the transaction
    QString transactionIDString = jsonObject["data"].toObject()["transaction"].toObject()["id"].toString();
    qDebug() << "Redeemed transaction with ID" << transactionIDString << "successfully.";
}

void OctreeServer::sendStatsPacket() {
    // TODO: we have too many stats to fit in a single MTU... so for now, we break it into multiple JSON objects and
    // send them separately. What we really should do is change the NodeList::sendStatsToDomainServer() to handle the
    // the following features:
    //    1) remember last state sent
    //    2) only send new data
    //    3) automatically break up into multiple packets
    static QJsonObject statsObject1;
    
    QString baseName = getMyServerName() + QString("Server");
    
    statsObject1[baseName + QString(".0.1.configuration")] = getConfiguration();
    
    statsObject1[baseName + QString(".0.2.detailed_stats_url")] = getStatusLink();
        
    statsObject1[baseName + QString(".0.3.uptime")] = getUptime();
    statsObject1[baseName + QString(".0.4.persistFileLoadTime")] = getFileLoadTime();
    statsObject1[baseName + QString(".0.5.clients")] = getCurrentClientCount();
    
    quint64 oneSecondAgo = usecTimestampNow() - USECS_PER_SECOND;
    
    statsObject1[baseName + QString(".0.6.threads.1.processing")] = (double)howManyThreadsDidProcess(oneSecondAgo);
    statsObject1[baseName + QString(".0.6.threads.2.packetDistributor")] = 
        (double)howManyThreadsDidPacketDistributor(oneSecondAgo);
    statsObject1[baseName + QString(".0.6.threads.3.handlePacektSend")] = 
        (double)howManyThreadsDidHandlePacketSend(oneSecondAgo);
    statsObject1[baseName + QString(".0.6.threads.4.writeDatagram")] = 
        (double)howManyThreadsDidCallWriteDatagram(oneSecondAgo);    
    
    statsObject1[baseName + QString(".1.1.octree.elementCount")] = (double)OctreeElement::getNodeCount();
    statsObject1[baseName + QString(".1.2.octree.internalElementCount")] = (double)OctreeElement::getInternalNodeCount();
    statsObject1[baseName + QString(".1.3.octree.leafElementCount")] = (double)OctreeElement::getLeafNodeCount();

    ThreadedAssignment::addPacketStatsAndSendStatsPacket(statsObject1);

    static QJsonObject statsObject2;

    statsObject2[baseName + QString(".2.outbound.data.totalPackets")] = (double)OctreeSendThread::_totalPackets;
    statsObject2[baseName + QString(".2.outbound.data.totalBytes")] = (double)OctreeSendThread::_totalBytes;
    statsObject2[baseName + QString(".2.outbound.data.totalBytesWasted")] = (double)OctreeSendThread::_totalWastedBytes;
    statsObject2[baseName + QString(".2.outbound.data.totalBytesOctalCodes")] = 
        (double)OctreePacketData::getTotalBytesOfOctalCodes();
    statsObject2[baseName + QString(".2.outbound.data.totalBytesBitMasks")] = 
        (double)OctreePacketData::getTotalBytesOfBitMasks();
    statsObject2[baseName + QString(".2.outbound.data.totalBytesBitMasks")] = (double)OctreePacketData::getTotalBytesOfColor();

    statsObject2[baseName + QString(".2.outbound.timing.1.avgLoopTime")] = getAverageLoopTime();
    statsObject2[baseName + QString(".2.outbound.timing.2.avgInsideTime")] = getAverageInsideTime();
    statsObject2[baseName + QString(".2.outbound.timing.3.avgTreeLockTime")] = getAverageTreeWaitTime();
    statsObject2[baseName + QString(".2.outbound.timing.4.avgEncodeTime")] = getAverageEncodeTime();
    statsObject2[baseName + QString(".2.outbound.timing.5.avgCompressAndWriteTime")] = getAverageCompressAndWriteTime();
    statsObject2[baseName + QString(".2.outbound.timing.5.avgSendTime")] = getAveragePacketSendingTime();
    statsObject2[baseName + QString(".2.outbound.timing.5.nodeWaitTime")] = getAverageNodeWaitTime();

    NodeList::getInstance()->sendStatsToDomainServer(statsObject2);

    static QJsonObject statsObject3;

    statsObject3[baseName + QString(".3.inbound.data.1.totalPackets")] = 
        (double)_octreeInboundPacketProcessor->getTotalPacketsProcessed();
    statsObject3[baseName + QString(".3.inbound.data.2.totalElements")] = 
        (double)_octreeInboundPacketProcessor->getTotalElementsProcessed();
    statsObject3[baseName + QString(".3.inbound.timing.1.avgTransitTimePerPacket")] = 
        (double)_octreeInboundPacketProcessor->getAverageTransitTimePerPacket();
    statsObject3[baseName + QString(".3.inbound.timing.2.avgProcessTimePerPacket")] = 
        (double)_octreeInboundPacketProcessor->getAverageProcessTimePerPacket();
    statsObject3[baseName + QString(".3.inbound.timing.3.avgLockWaitTimePerPacket")] = 
        (double)_octreeInboundPacketProcessor->getAverageLockWaitTimePerPacket();
    statsObject3[baseName + QString(".3.inbound.timing.4.avgProcessTimePerElement")] = 
        (double)_octreeInboundPacketProcessor->getAverageProcessTimePerElement();
    statsObject3[baseName + QString(".3.inbound.timing.5.avgLockWaitTimePerElement")] = 
        (double)_octreeInboundPacketProcessor->getAverageLockWaitTimePerElement();

    NodeList::getInstance()->sendStatsToDomainServer(statsObject3);
}

QMap<OctreeSendThread*, quint64> OctreeServer::_threadsDidProcess;
QMap<OctreeSendThread*, quint64> OctreeServer::_threadsDidPacketDistributor;
QMap<OctreeSendThread*, quint64> OctreeServer::_threadsDidHandlePacketSend;
QMap<OctreeSendThread*, quint64> OctreeServer::_threadsDidCallWriteDatagram;

QMutex OctreeServer::_threadsDidProcessMutex;
QMutex OctreeServer::_threadsDidPacketDistributorMutex;
QMutex OctreeServer::_threadsDidHandlePacketSendMutex;
QMutex OctreeServer::_threadsDidCallWriteDatagramMutex;


void OctreeServer::didProcess(OctreeSendThread* thread) {
    QMutexLocker locker(&_threadsDidProcessMutex);
    _threadsDidProcess[thread] = usecTimestampNow();
}

void OctreeServer::didPacketDistributor(OctreeSendThread* thread) {
    QMutexLocker locker(&_threadsDidPacketDistributorMutex);
    _threadsDidPacketDistributor[thread] = usecTimestampNow();
}

void OctreeServer::didHandlePacketSend(OctreeSendThread* thread) {
    QMutexLocker locker(&_threadsDidHandlePacketSendMutex);
    _threadsDidHandlePacketSend[thread] = usecTimestampNow();
}

void OctreeServer::didCallWriteDatagram(OctreeSendThread* thread) {
    QMutexLocker locker(&_threadsDidCallWriteDatagramMutex);
    _threadsDidCallWriteDatagram[thread] = usecTimestampNow();
}


void OctreeServer::stopTrackingThread(OctreeSendThread* thread) {
    QMutexLocker lockerA(&_threadsDidProcessMutex);
    QMutexLocker lockerB(&_threadsDidPacketDistributorMutex);
    QMutexLocker lockerC(&_threadsDidHandlePacketSendMutex);
    QMutexLocker lockerD(&_threadsDidCallWriteDatagramMutex);

    _threadsDidProcess.remove(thread);
    _threadsDidPacketDistributor.remove(thread);
    _threadsDidHandlePacketSend.remove(thread);
    _threadsDidCallWriteDatagram.remove(thread);
}

int howManyThreadsDidSomething(QMutex& mutex, QMap<OctreeSendThread*, quint64>& something, quint64 since) {
    int count = 0;
    if (mutex.tryLock()) {
        if (since == 0) {
            count = something.size();
        } else {
            QMap<OctreeSendThread*, quint64>::const_iterator i = something.constBegin();
            while (i != something.constEnd()) {
                if (i.value() > since) {
                    count++;
                }
                ++i;
            }
        }
        mutex.unlock();
    }
    return count;
}


int OctreeServer::howManyThreadsDidProcess(quint64 since) {
    return howManyThreadsDidSomething(_threadsDidProcessMutex, _threadsDidProcess, since);
}

int OctreeServer::howManyThreadsDidPacketDistributor(quint64 since) {
    return howManyThreadsDidSomething(_threadsDidPacketDistributorMutex, _threadsDidPacketDistributor, since);
}

int OctreeServer::howManyThreadsDidHandlePacketSend(quint64 since) {
    return howManyThreadsDidSomething(_threadsDidHandlePacketSendMutex, _threadsDidHandlePacketSend, since);
}

int OctreeServer::howManyThreadsDidCallWriteDatagram(quint64 since) {
    return howManyThreadsDidSomething(_threadsDidCallWriteDatagramMutex, _threadsDidCallWriteDatagram, since);
}

