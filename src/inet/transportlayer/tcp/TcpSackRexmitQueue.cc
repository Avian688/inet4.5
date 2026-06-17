//
// Copyright (C) 2009-2010 Thomas Reschka
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/transportlayer/tcp/TcpSackRexmitQueue.h"

namespace inet {

namespace tcp {

TcpSackRexmitQueue::TcpSackRexmitQueue()
{
    conn = nullptr;
    begin = end = 0;

    m_sentSize = 0;
    m_sackedOut = 0;
    m_lostOut = 0;
    m_totalDetectedLostBytes = 0;
    m_retrans = 0;
    clearRecentLossSample();
}

TcpSackRexmitQueue::~TcpSackRexmitQueue()
{
    while (!rexmitQueue.empty())
        rexmitQueue.pop_front();
}

void TcpSackRexmitQueue::init(uint32_t seqNum)
{
    begin = seqNum;
    end = seqNum;
    rexmitQueue.clear();
    rexmitMap.clear();
    backupRexmitMap.clear();

    m_sentSize = 0;
    m_sackedOut = 0;
    m_lostOut = 0;
    m_totalDetectedLostBytes = 0;
    m_retrans = 0;
    clearRecentLossSample();
}

std::string TcpSackRexmitQueue::str() const
{
    std::stringstream out;

    out << "[" << getBufferStartSeq() << ".." << getBufferEndSeq() << ")";
    return out.str();
}

std::string TcpSackRexmitQueue::detailedInfo() const
{
    std::stringstream out;
    out << str() << endl;

    uint j = 1;

    if (m_updatedSackEnabled) {
        for (auto elem : rexmitMap) {
            out << j << ". region: [" << elem.second.beginSeqNum << ".." << elem.second.endSeqNum
                << ") \t sacked=" << elem.second.sacked << "\t rexmitted=" << elem.second.rexmitted
                << endl;
            j++;
        }
    }
    else {
        for (const auto& elem : rexmitQueue) {
            out << j << ". region: [" << elem.beginSeqNum << ".." << elem.endSeqNum
                << ") \t sacked=" << elem.sacked << "\t rexmitted=" << elem.rexmitted
                << endl;
            j++;
        }
    }
    return out.str();
}

void TcpSackRexmitQueue::discardUpTo(uint32_t seqNum)
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, seqNum) && seqLE(seqNum, end));

        if (!rexmitQueue.empty()) {
            auto i = rexmitQueue.begin();

            while ((i != rexmitQueue.end()) && seqLE(i->endSeqNum, seqNum))
                i = rexmitQueue.erase(i);

            if (i != rexmitQueue.end()) {
                ASSERT(seqLE(i->beginSeqNum, seqNum) && seqLess(seqNum, i->endSeqNum));
                i->beginSeqNum = seqNum;
            }
        }

        begin = seqNum;
        ASSERT(checkQueue());
        return;
    }

//    if(!(seqLE(begin, seqNum) && seqLE(seqNum, end))){
//        std::cout << "\n ASSERT in discardUpTo FAILED" << endl;
//        std::cout << "\n" << detailedInfo() << endl;
//    }
//    ASSERT(seqLE(begin, seqNum) && seqLE(seqNum, (--rexmitMap.end())->second.endSeqNum));
//    if (!rexmitQueue.empty()) {
//        auto i = rexmitQueue.begin();
//        while ((i != rexmitQueue.end()) && seqLE(i->endSeqNum, seqNum))
//        {
//            rexmitMap.erase(i->endSeqNum);
//            i = rexmitQueue.erase(i);
//        }
//        if (i != rexmitQueue.end()) {
//            //shift begin to seqNum. Will shift large first blocks beginSeqNum up to seqNum.
//            ASSERT(seqLE(i->beginSeqNum, seqNum) && seqLess(seqNum, i->endSeqNum));
//            i->beginSeqNum = seqNum;
//        }
//    }
    if (!rexmitMap.empty()) {
            auto i = rexmitMap.begin();
//            while ((i != rexmitMap.end()) && seqLE(i->second.endSeqNum, seqNum))
//            {
//                rexmitMap.erase(i->second.endSeqNum);
//                i++;
//            }
            //std::cout << "\n DISCARDING UP TO: " << seqNum << endl;
            while ((i != rexmitMap.end()) && seqLE(i->second.endSeqNum, seqNum)){
                    m_sentSize -= i->second.endSeqNum - i->second.beginSeqNum;
                    if(i->second.sacked){
                        m_sackedOut -= i->second.endSeqNum - i->second.beginSeqNum;
                    }

                    if(i->second.rexmitted){
                        m_retrans -= i->second.endSeqNum - i->second.beginSeqNum;
                    }

                    if(i->second.lost){
                        m_lostOut -= i->second.endSeqNum - i->second.beginSeqNum;
                    }
                    i = rexmitMap.erase(i);
            }
            if (i != rexmitMap.end() && seqLess(i->second.beginSeqNum, seqNum) && seqLess(seqNum, i->second.endSeqNum)) {
                const uint32_t ackedBytes = seqNum - i->second.beginSeqNum;
                m_sentSize -= ackedBytes;
                if (i->second.sacked)
                    m_sackedOut -= ackedBytes;
                if (i->second.rexmitted)
                    m_retrans -= ackedBytes;
                if (i->second.lost)
                    m_lostOut -= ackedBytes;
                i->second.beginSeqNum = seqNum;
            }
//            if (i != iEnd) {
//                //ASSERT(seqLE(i->second.beginSeqNum, seqNum) && seqLess(seqNum, i->second.endSeqNum));
//                m_sentSize -= seqNum - i->second.beginSeqNum;
//                if(i->second.sacked){
//                    m_sackedOut -= seqNum - i->second.beginSeqNum;
//                }
//
//                if(i->second.rexmitted){
//                    m_retrans -= seqNum - i->second.beginSeqNum;
//                }
//
//                if(i->second.lost){
//                    m_lostOut -= seqNum - i->second.beginSeqNum;
//                }
//
//                i->second.beginSeqNum = seqNum;
//            }
    }
    begin = seqNum;

    if (!rexmitMap.empty() && rexmitMap.begin()->second.sacked) {
        std::cout << "\n SACK ERROR HERE" << endl;
    }
    // TESTING queue:
    //ASSERT(checkQueue());
}

std::list<uint32_t> TcpSackRexmitQueue::getDiscardList(uint32_t seqNum) //TODO MAKE MORE EFFICIENT
{
    std::list<uint32_t> skbDeliveredList;
    if (!m_updatedSackEnabled) {
        for (const auto& elem : rexmitQueue) {
            if (seqLE(elem.endSeqNum, seqNum))
                skbDeliveredList.push_back(elem.endSeqNum);
            else
                break;
        }
        return skbDeliveredList;
    }

    if (!rexmitMap.empty()) {
        auto i = rexmitMap.begin();
        while ((i != rexmitMap.end()) && seqLE(i->second.endSeqNum, seqNum)){
            skbDeliveredList.push_back(i->second.endSeqNum);
            i++;
        }
    }
    return skbDeliveredList;
}

void TcpSackRexmitQueue::enqueueSentData(uint32_t fromSeqNum, uint32_t toSeqNum)
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

        bool found = false;
        Region region;

        EV_INFO << "rexmitQ: " << str() << " enqueueSentData [" << fromSeqNum << ".." << toSeqNum << ")\n";
        ASSERT(seqLess(fromSeqNum, toSeqNum));

        if (rexmitQueue.empty() || (end == fromSeqNum)) {
            region.beginSeqNum = fromSeqNum;
            region.endSeqNum = toSeqNum;
            region.sacked = false;
            region.rexmitted = false;
            region.lost = false;
            rexmitQueue.push_back(region);
            found = true;
            fromSeqNum = toSeqNum;
        }
        else {
            auto i = rexmitQueue.begin();

            while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
                i++;

            ASSERT(i != rexmitQueue.end());
            ASSERT(seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

            if (i->beginSeqNum != fromSeqNum) {
                region = *i;
                region.endSeqNum = fromSeqNum;
                rexmitQueue.insert(i, region);
                i->beginSeqNum = fromSeqNum;
            }

            while (i != rexmitQueue.end() && seqLE(i->endSeqNum, toSeqNum)) {
                i->rexmitted = true;
                fromSeqNum = i->endSeqNum;
                found = true;
                i++;
            }

            if (fromSeqNum != toSeqNum) {
                bool beforeEnd = (i != rexmitQueue.end());
                ASSERT(i == rexmitQueue.end() || seqLess(i->beginSeqNum, toSeqNum));

                region.beginSeqNum = fromSeqNum;
                region.endSeqNum = toSeqNum;
                region.sacked = beforeEnd ? i->sacked : false;
                region.rexmitted = beforeEnd;
                region.lost = false;
                rexmitQueue.insert(i, region);
                found = true;
                fromSeqNum = toSeqNum;

                if (beforeEnd)
                    i->beginSeqNum = toSeqNum;
            }
        }

        ASSERT(fromSeqNum == toSeqNum);

        if (!found)
            EV_DEBUG << "Not found enqueueSentData(" << fromSeqNum << ", " << toSeqNum << ")\nThe Queue is:\n" << detailedInfo();

        ASSERT(found);

        begin = rexmitQueue.front().beginSeqNum;
        end = rexmitQueue.back().endSeqNum;
        ASSERT(checkQueue());
        return;
    }

    //std::cout << "\n BEGIN: " << begin << endl;
    //std::cout << "\n END: " << end << endl;

//    if(simTime().dbl() > 2){
//        std::cout << "\n BOUND CHECK ONE: " << boundTimeCheckOne << endl;
//        std::cout << "\n WHILE CHECK ONE: " << whileTimeCheckOne << endl;
//        std::cout << "\n BOUND CHECK TWO: " << boundTimeCheckTwo << endl;
//        std::cout << "\n WHILE CHECK TWO: " << whileTimeCheckTwo << endl;
//    }
    //std::cout << "\n SACK PACKET SIZE: " << toSeqNum - fromSeqNum << endl;
    ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

    const uint32_t originalFromSeqNum = fromSeqNum;
    const uint32_t originalToSeqNum = toSeqNum;
    bool found = false;
    Region region;
    region. m_delivered = 0;
    region. m_firstSentTime = 0;
    region.m_lastSentTime = 0;
    region.m_deliveredTime = 0;
    region.m_bytes = 0;
    region.m_txInFlight = 0;
    region.m_isAppLimited = 0;

    EV_INFO << "rexmitQ: " << str() << " enqueueSentData [" << fromSeqNum << ".." << toSeqNum << ")\n";

    //std::cout << "\n map size: " << rexmitMap.size() << endl;
//    if(rexmitMap.size() > 3000){
//        std::cout << detailedInfo() << endl;
//    }
    ASSERT(seqLess(fromSeqNum, toSeqNum));


    if (rexmitMap.empty() || (end == fromSeqNum)) { //where most of the blocks are pushed onto the scoreboard. When added, pop prev element (if not rexmitted or sackeed) and extend said region.
        region.beginSeqNum = fromSeqNum;
        region.endSeqNum = toSeqNum;
        region.sacked = false;
        region.rexmitted = false;
        region.lost = false;

        //region.numOfDiscontiguousSacks = 0;
        m_sentSize += toSeqNum - fromSeqNum;
        rexmitMap.insert({toSeqNum, region});
//        if(!(rexmitMap.empty())){
//            if((fromSeqNum == (--rexmitMap.end())->second.endSeqNum) && (!((--rexmitMap.end())->second.sacked) && !((--rexmitMap.end())->second.rexmitted) && !((--rexmitMap.end())->second.lost) )){  //extending current region TODO add check to ensure beginSeqNum of new block starts where the previous block ends
//                //std::cout << "\n EXTENDING BLOCK" << endl;
//                Region dupRegion;
//                dupRegion.beginSeqNum = (--rexmitMap.end())->second.beginSeqNum;
//                dupRegion.endSeqNum = toSeqNum;
//                dupRegion.sacked = (--rexmitMap.end())->second.sacked;
//                dupRegion.rexmitted = (--rexmitMap.end())->second.rexmitted;
//                dupRegion.lost = (--rexmitMap.end())->second.lost;
//                dupRegion.numOfDiscontiguousSacks = (--rexmitMap.end())->second.numOfDiscontiguousSacks;
//
//                rexmitMap.erase((--rexmitMap.end())->second.endSeqNum); //look into optimise erase methods to use it
//                //rexmitQueue.back().endSeqNum = toSeqNum;
//                rexmitMap.insert({toSeqNum, dupRegion});
//                m_sentSize += toSeqNum - fromSeqNum;
//                //TODO remove map element and add new with updated endSeqNumber!!!
//            }
//            else{
//                rexmitMap.insert({toSeqNum, region});
//                m_sentSize += toSeqNum - fromSeqNum;
//            }
//        }
//        else{
//            rexmitMap.insert({toSeqNum, region});
//            m_sentSize += toSeqNum - fromSeqNum;
//        }
        found = true;
        fromSeqNum = toSeqNum;
    }
    else {
        //auto i = rexmitQueue.begin();

        //while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
        //    i++;

        auto iter = rexmitMap.upper_bound(fromSeqNum);

        ASSERT(iter != rexmitMap.end());
        ASSERT(seqLE(iter->second.beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, iter->second.endSeqNum));
        //ASSERT(i != rexmitQueue.end());
        //ASSERT(seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

        if (iter->second.beginSeqNum != fromSeqNum) {// is this used? Ignore this edge case for now! TODO
            // chunk item
            std::cout << "\n WEIRD CASE HAPPENING" << endl;
            region = iter->second; //TODO CHECK THIS
            region.endSeqNum = fromSeqNum;
            //rexmitQueue.insert(i, region);
            m_sentSize += toSeqNum - fromSeqNum;
            rexmitMap.insert({region.endSeqNum, region});
            iter->second.beginSeqNum = fromSeqNum;
        }

        while (iter != rexmitMap.end() && seqLE(iter->second.endSeqNum, toSeqNum)) { //Called when you are retransmitting a block from the map.
            if(!iter->second.rexmitted){
                m_retrans += iter->second.endSeqNum - iter->second.beginSeqNum;
            }
            //std::cout << "\n MARKING AS RETRANSMITTED AT SIMTIME: " << simTime() << " SeqNo: " << iter->second.endSeqNum << endl;
            iter->second.rexmitted = true;
            EV_INFO << "rexmitQ: " << " Retransmitting [" << fromSeqNum << ".." << toSeqNum << ")\n";
            //iter->second.lost = false;
            fromSeqNum = iter->second.endSeqNum;
            found = true;
            iter++;
        }

        if (fromSeqNum != toSeqNum) { //rarely called? this is called if the block does not exist AND fromSeqNum is not the end of the current queue
            std::cout << "\n WEIRD CASE HAPPENING 2" << endl;
            bool beforeEnd = (iter != rexmitMap.end());
            //ASSERT(i == rexmitQueue.end() || seqLess(i->beginSeqNum, toSeqNum));
            ASSERT(iter == rexmitMap.end() || seqLess(iter->second.beginSeqNum, toSeqNum));

            region.beginSeqNum = fromSeqNum;
            region.endSeqNum = toSeqNum;
            region.sacked = beforeEnd ? iter->second.sacked : false; //splitting so must have same sack as previous block
            region.rexmitted = beforeEnd;

//            if(region.rexmitted == true){
//                region.lost = false;
//            }
            //rexmitQueue.insert(i, region);
            rexmitMap.insert({toSeqNum, region});
            //std::cout << "\n Inserting this block to scoreboard..." << endl;
            found = true;
            fromSeqNum = toSeqNum;

            if (beforeEnd)
                iter->second.beginSeqNum = toSeqNum;
        }
    }

    ASSERT(fromSeqNum == toSeqNum);

    if (!found) {
        EV_DEBUG << "Not found enqueueSentData(" << fromSeqNum << ", " << toSeqNum << ")\nThe Queue is:\n" << detailedInfo();
    }

    ASSERT(found);

    if (rexmitMap.empty()) {
        EV_WARN << "enqueueSentData left the updated SACK retransmission map empty; "
                << "falling back to the requested sent range.\n";
        begin = originalFromSeqNum;
        end = originalToSeqNum;
        return;
    }

    begin = rexmitMap.begin()->second.beginSeqNum;
    end = rexmitMap.rbegin()->second.endSeqNum;
    consistencyCheck();
}

bool TcpSackRexmitQueue::checkQueue() const
{
    uint32_t b = begin;
    bool f = true;

    for (const auto& elem : rexmitQueue) {
        f = f && (b == elem.beginSeqNum);
        f = f && seqLess(elem.beginSeqNum, elem.endSeqNum);
        b = elem.endSeqNum;
    }

    f = f && (b == end);

    if (!f) {
        EV_DEBUG << "Invalid Queue\nThe Queue is:\n" << detailedInfo();
    }

    return f;
}

void TcpSackRexmitQueue::setSackedBit(uint32_t fromSeqNum, uint32_t toSeqNum)
{
    if (!m_updatedSackEnabled) {
        if (seqLess(fromSeqNum, begin))
            fromSeqNum = begin;

        ASSERT(seqLess(fromSeqNum, end));
        ASSERT(seqLess(begin, toSeqNum) && seqLE(toSeqNum, end));
        ASSERT(seqLess(fromSeqNum, toSeqNum));

        bool found = false;

        if (!rexmitQueue.empty()) {
            auto i = rexmitQueue.begin();

            while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
                i++;

            ASSERT(i != rexmitQueue.end() && seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

            if (i->beginSeqNum != fromSeqNum) {
                Region region = *i;
                region.endSeqNum = fromSeqNum;
                rexmitQueue.insert(i, region);
                i->beginSeqNum = fromSeqNum;
            }

            while (i != rexmitQueue.end() && seqLE(i->endSeqNum, toSeqNum)) {
                if (seqGE(i->beginSeqNum, fromSeqNum)) {
                    found = true;
                    i->sacked = true;
                }
                i++;
            }

            if (i != rexmitQueue.end() && seqLess(i->beginSeqNum, toSeqNum) && seqLess(toSeqNum, i->endSeqNum)) {
                Region region = *i;
                region.endSeqNum = toSeqNum;
                region.sacked = true;
                rexmitQueue.insert(i, region);
                i->beginSeqNum = toSeqNum;
            }
        }

        if (!found)
            EV_DETAIL << "FAILED to set sacked bit for region: [" << fromSeqNum << ".." << toSeqNum << "). Not found in retransmission queue.\n";

        ASSERT(checkQueue());
        return;
    }

    if (rexmitMap.empty()) {
        EV_DETAIL << "FAILED to set sacked bit for region: [" << fromSeqNum << ".." << toSeqNum
                  << "). Retransmission queue is empty.\n";
        return;
    }

    const uint32_t mapBegin = rexmitMap.begin()->second.beginSeqNum;
    const uint32_t mapEnd = rexmitMap.rbegin()->second.endSeqNum;

    if (seqLess(fromSeqNum, mapBegin))
        fromSeqNum = mapBegin;

    ASSERT(seqLess(fromSeqNum, mapEnd));
    ASSERT(seqLess(mapBegin, toSeqNum) && seqLE(toSeqNum, mapEnd));
    ASSERT(seqLess(fromSeqNum, toSeqNum));

    bool found = false;

    //uint32_t seq = fromSeqNum + 1488;

    if (!rexmitMap.empty()) {

        auto iter = rexmitMap.begin();
        uint32_t beginOfCurrentPacket = iter->second.beginSeqNum;

        if(beginOfCurrentPacket + m_sentSize < fromSeqNum){
            std::cout << "\n RETURNING ITS HAPPENING" << endl;
            return;
        }

        while (iter != rexmitMap.end() && seqLE(iter->second.endSeqNum, toSeqNum)){// && seqLE(iter->second.endSeqNum, toSeqNum) && seqGE(iter->second.beginSeqNum, fromSeqNum)){

            found = true;
            Region& region = iter->second;
            uint32_t packetSize = region.endSeqNum - region.beginSeqNum;
            if(seqGE(region.beginSeqNum, fromSeqNum)){
                if(!region.sacked){
                    if(region.lost == true){
                       m_lostOut -= packetSize;
                       region.lost = false;
                    }

                    if (region.rexmitted) {
                       m_retrans -= packetSize;
                       region.rexmitted = false;
                    }

                    m_sackedOut += packetSize;
                    region.sacked = true; // set sacked bit //may need to merge
                }
                if(region.lost == true) {
                    m_lostOut -= packetSize;
                    region.lost = false;
                }
            }
            ++iter;
        }
//        for (uint32_t seqNo = seq; seqNo <= toSeqNum; seqNo += 1488) {
//            if(findRegion(seqNo))
//            {
//                auto& iter = rexmitMap.at(seqNo);
//                if(!iter.sacked){
//                    if(iter.lost == true)
//                    {
//                       m_lostOut -= iter.endSeqNum - iter.beginSeqNum;
//                       iter.lost = false;
//                    }
//                    iter.sacked = true;
//                    m_sackedOut += iter.endSeqNum - iter.beginSeqNum;
//                }
//            }
//        }
    }

//
//    if (!rexmitMap.empty()) {
//        //auto i = rexmitQueue.begin();
//
//        //while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
//        //    i++;
//        auto iter = rexmitMap.upper_bound(fromSeqNum);
//
//        ASSERT(iter != rexmitMap.end() && seqLE(iter->second.beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, iter->second.endSeqNum));
//        //ASSERT(i != rexmitQueue.end() && seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));
//
//        //std::cout << "\n SET SACKET BIT" << endl;
//        //std::cout << "\n fromSeqNum " << fromSeqNum << endl;
//        //std::cout << "\n toSeqNum " << toSeqNum << endl;
//        //std::cout << "\n i->beginSeqNum " << i->beginSeqNum << endl;
//        //std::cout << "\n i->endSeqNum " << i->endSeqNum << endl;
//        bool firstTest = seqGreater(fromSeqNum, iter->second.beginSeqNum);
//        bool secondTest = seqLess(toSeqNum, iter->second.endSeqNum);
//        if (iter->second.beginSeqNum != fromSeqNum) {
////            if(seqGreater(fromSeqNum, iter->second.beginSeqNum) && seqLE(toSeqNum, iter->second.endSeqNum)){ //Do not forget case where it is last block (toSeqNum == i->endSeqNum)
////                //std::cout << "\n FOUND BLOCK TO BE SACKED, WITHIN LARGER BLOCK" << endl;
////                uint32_t oldRegionEnd = iter->second.endSeqNum;
////
//////                std::cout << "\n BEFORE ADD: " << endl;
//////                for(auto it = rexmitMap.cbegin(); it != rexmitMap.cend(); ++it)
//////                {
//////                    std::cout << "KEY: "<< it->first << "\n";
//////                    std::cout << "beginSeqNum" << it->second.beginSeqNum << "\n";
//////                    std::cout << "endSeqNum" << it->second.endSeqNum << "\n";
//////                    std::cout << "sacked" << it->second.sacked << "\n";
//////                    std::cout << "rexmitted" << it->second.rexmitted << "\n";
//////                }
////
////                bool rex = false;
////                bool los = false;
////                Region startRegion;
////                startRegion.beginSeqNum = iter->second.beginSeqNum;
////                startRegion.endSeqNum = fromSeqNum;
////                startRegion.sacked = iter->second.sacked;
////                startRegion.rexmitted = iter->second.rexmitted;
////                rex = iter->second.rexmitted;
////                startRegion.lost = iter->second.lost;
////                los = iter->second.lost;
////
////                rexmitMap.erase(iter->second.endSeqNum);
////                //i->endSeqNum = fromSeqNum;
////
////                rexmitMap.insert({fromSeqNum, startRegion}); //potential merging
////
////                Region region;
////                region.beginSeqNum = fromSeqNum;
////                region.endSeqNum = toSeqNum;
////                if(!startRegion.sacked){
////                    m_sackedOut += toSeqNum - fromSeqNum;
////                }
////                region.sacked = true;
////                region.rexmitted = false;
////                region.lost = false;
////                //iter++;                                                                     //TODO Fix this error
////                //rexmitQueue.insert(i,region);
////                rexmitMap.insert({toSeqNum, region});
////
////                Region endRegion;
////                endRegion.beginSeqNum = toSeqNum;
////                endRegion.endSeqNum = oldRegionEnd;
////                endRegion.sacked = startRegion.sacked;
////                endRegion.rexmitted = startRegion.rexmitted;
////                endRegion.lost = startRegion.lost;
////                //rexmitQueue.insert(i,endRegion);
////                rexmitMap.insert({oldRegionEnd, endRegion});
////
////                //get start seqNum of large block
////                //get end seqNum of large black
////                //
////            }
////            else{
//                //Gap in queue (should rarely if at all occur)
//            Region region = iter->second;
//            region.endSeqNum = fromSeqNum;
//            //rexmitQueue.insert(i, region);
//            rexmitMap.insert({region.endSeqNum, region}); //TODO potentially fix
//            iter->second.beginSeqNum = fromSeqNum;
//            //}
//        }
//
//        iter = rexmitMap.upper_bound(fromSeqNum); //sets sacked bit if the following regions of the above check are also within the range
//        while ((iter != rexmitMap.end() && seqLE(iter->second.endSeqNum, toSeqNum))) {  //Most common case. Should split existing large region if it exists into smaller region
//            if (seqGE(iter->second.beginSeqNum, fromSeqNum)) { // Search region in queue!
//                found = true;
//                if(!iter->second.sacked){
//                    if(iter->second.lost == true){
//                       m_lostOut -= iter->second.endSeqNum - iter->second.beginSeqNum;
//                       iter->second.lost = false;
//                    }
//
//                    m_sackedOut += iter->second.endSeqNum - iter->second.beginSeqNum;
//                    iter->second.sacked = true; // set sacked bit //may need to merge
//                }
//
//                //iter->second.lost = false; CHANGEDRECNTLY
//                //std::cout << "\n Found block, setting sacked bit!" << endl;
//                //std::cout << "\n"<<  detailedInfo() << endl;
//            }
////            std::cout << "\n iter->second.beginSeqNum: " << iter->second.beginSeqNum << endl;
////            std::cout << "\n iter->second.endSeqNum: " << iter->second.endSeqNum << endl;
////            std::cout << "\n fromSeqNum: " << fromSeqNum << endl;
////            std::cout << "\n toSeqNum: " << toSeqNum << endl;
////            std::cout << "\n"<<  detailedInfo() << endl;
//            iter++;
//        }
//        //splits last element
////        if (iter != rexmitMap.end() && seqLess(iter->second.beginSeqNum, toSeqNum) && seqLess(toSeqNum, iter->second.endSeqNum)) { //Edge case, rarely happens ignore for now
////            Region region = iter->second;
////            //std::cout << "\n ALTERNATE Found block, setting sacked bit!" << endl;
////            region.endSeqNum = toSeqNum;
////            if(!region.sacked){
////                m_sackedOut += iter->second.endSeqNum - iter->second.beginSeqNum;
////            }
////            std::cout << "\n EDGE CASE BEING CALLED" << endl;
////            //iter->second.sacked = true; // set sacked bit //may need to merge
////            region.sacked = true;
////            region.lost = false;
////            //rexmitQueue.insert(i, region);
////            rexmitMap.insert({region.endSeqNum, region});
////            iter->second.beginSeqNum = toSeqNum; //TODO MAYBE FIX
////        }
//    }

    if (!found)
        EV_DETAIL << "FAILED to set sacked bit for region: [" << fromSeqNum << ".." << toSeqNum << "). Not found in retransmission queue.\n";


    //ASSERT(checkQueue());
}

std::list<uint32_t> TcpSackRexmitQueue::setSackedBitList(uint32_t fromSeqNum, uint32_t toSeqNum)
{
    if (!m_updatedSackEnabled) {
        std::list<uint32_t> skbDeliveredList;
        setSackedBit(fromSeqNum, toSeqNum);
        return skbDeliveredList;
    }

    std::list<uint32_t> skbDeliveredList;

    if (rexmitMap.empty())
        return skbDeliveredList;

    const uint32_t mapBegin = rexmitMap.begin()->second.beginSeqNum;
    const uint32_t mapEnd = rexmitMap.rbegin()->second.endSeqNum;

    if (seqLess(fromSeqNum, mapBegin))
        fromSeqNum = mapBegin;

    ASSERT(seqLess(fromSeqNum, mapEnd));
    ASSERT(seqLess(mapBegin, toSeqNum) && seqLE(toSeqNum, mapEnd));
    ASSERT(seqLess(fromSeqNum, toSeqNum));

    if (!rexmitMap.empty()) {

        auto iter = rexmitMap.begin();
        uint32_t beginOfCurrentPacket = iter->second.beginSeqNum;

        if(beginOfCurrentPacket + m_sentSize < fromSeqNum){
            std::cout << "\n RETURNING ITS HAPPENING" << endl;
            return skbDeliveredList;
        }

        while (iter != rexmitMap.end()){// && seqLE(iter->second.endSeqNum, toSeqNum) && seqGE(iter->second.beginSeqNum, fromSeqNum)){
            Region& region = iter->second;
            uint32_t packetSize = region.endSeqNum - region.beginSeqNum;
            if(seqGE(region.beginSeqNum, fromSeqNum) && seqLE(region.endSeqNum, toSeqNum)){
                if(region.sacked){
                    //Do nothing
                }
                else{
                    if(region.lost == true) {
                        region.lost = false;
                        m_lostOut -= packetSize;
                    }

                    if (region.rexmitted) {
                        region.rexmitted = false;
                        m_retrans -= packetSize;
                    }

                    m_sackedOut += packetSize;
                    region.sacked = true; // set sacked bit //may need to merge

                    skbDeliveredList.push_back(region.endSeqNum);
                }
            }
            else if(seqGE(region.endSeqNum, toSeqNum)){
                return skbDeliveredList;
            }
            ++iter;
        }
    }
    return skbDeliveredList;
}

bool TcpSackRexmitQueue::getSackedBit(uint32_t seqNum) //const
{
    ASSERT(seqLE(begin, seqNum) && seqLE(seqNum, end));

    if (!m_updatedSackEnabled) {
        RexmitQueue::const_iterator i = rexmitQueue.begin();

        if (end == seqNum)
            return false;

        while (i != rexmitQueue.end() && seqLE(i->endSeqNum, seqNum))
            i++;

        ASSERT((i != rexmitQueue.end()) && seqLE(i->beginSeqNum, seqNum) && seqLess(seqNum, i->endSeqNum));
        return i->sacked;
    }

    if (end == seqNum){
        std::cout << "\n WHAT IS HAPPENING HERE" << endl;
        return false;
    }
    auto iter = rexmitMap.upper_bound(seqNum);
//    while (i != rexmitQueue.end() && seqLE(i->endSeqNum, seqNum))
//        i++;

    ASSERT((iter != rexmitMap.end()) && seqLE(iter->second.beginSeqNum, seqNum) && seqLess(seqNum, iter->second.endSeqNum));


    return iter->second.sacked;
}

uint32_t TcpSackRexmitQueue::getHighestSackedSeqNum() //const
{
    if (!m_updatedSackEnabled) {
        for (RexmitQueue::const_reverse_iterator i = rexmitQueue.rbegin(); i != rexmitQueue.rend(); i++) {
            if (i->sacked)
                return i->endSeqNum;
        }
        return begin;
    }

    for (auto iter = rexmitMap.rbegin(); iter != rexmitMap.rend(); iter++) {
        if (iter->second.sacked){
            return iter->second.endSeqNum;
        }
    }
    return rexmitMap.empty() ? begin : rexmitMap.begin()->second.beginSeqNum;//+1488;
}

uint32_t TcpSackRexmitQueue::getHighestRexmittedSeqNum() //const
{
    if (!m_updatedSackEnabled) {
        for (RexmitQueue::const_reverse_iterator i = rexmitQueue.rbegin(); i != rexmitQueue.rend(); i++) {
            if (i->rexmitted)
                return i->endSeqNum;
        }
        return begin;
    }

//    if(simTime() > 99.8){
//         std::cout << "\n TOTALS: \n";
//         std::cout << "discardUpTo TIME: " <<  discardUpToTime << "\n";
//         std::cout << "enqueueSentData TIME: " <<  enqueueSentDataTime << "\n";
//         std::cout << "setSackedBit TIME: " <<  setSackedBitTime << "\n";
//         std::cout << "getSackedBit TIME: " <<  getSackedBitTime << "\n";
//         std::cout << "getHighestSackedSeqNum TIME: " <<  getHighestSackedSeqNumTime << "\n";
//         std::cout << "getHighestRexmittedSeqNum TIME: " <<  getHighestRexmittedSeqNumTime << "\n";
//         std::cout << "checkRexmitQueueForSackedOrRexmittedSegments TIME: " <<  checkRexmitQueueForSackedOrRexmittedSegmentsTime << "\n";
//         std::cout << "getTotalAmountOfSackedBytes TIME: " <<  getTotalAmountOfSackedBytesTime << "\n";
//         std::cout << "getAmountOfSackedBytes TIME: " <<  getAmountOfSackedBytesTime << "\n";
//         std::cout << "getNumOfDiscontiguousSacks TIME: " <<  getNumOfDiscontiguousSacksTime << "\n";
//         std::cout << "checkSackBlock TIME: " <<  checkSackBlockTime << "\n";
//     }
        for (auto iter = rexmitMap.rbegin(); iter != rexmitMap.rend(); iter++) {
            //std::cout << "\n In getHighestRexmittedSeqNum() " << "\n";
            if (iter->second.rexmitted){
                return iter->second.endSeqNum;
            }
        }
        return rexmitMap.empty() ? begin : rexmitMap.begin()->second.beginSeqNum;//+1488;
}

uint32_t TcpSackRexmitQueue::checkRexmitQueueForSackedOrRexmittedSegments(uint32_t fromSeqNum) //const
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

        if (rexmitQueue.empty() || (end == fromSeqNum))
            return 0;

        RexmitQueue::const_iterator i = rexmitQueue.begin();
        uint32_t bytes = 0;

        while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
            i++;

        while (i != rexmitQueue.end() && ((i->sacked || i->rexmitted))) {
            ASSERT(seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

            bytes += (i->endSeqNum - fromSeqNum);
            fromSeqNum = i->endSeqNum;
            i++;
        }

        return bytes;
    }

    if (rexmitMap.empty())
        return 0;

    const uint32_t mapBegin = rexmitMap.begin()->second.beginSeqNum;
    const uint32_t mapEnd = rexmitMap.rbegin()->second.endSeqNum;

    ASSERT(seqLE(mapBegin, fromSeqNum) && seqLE(fromSeqNum, mapEnd));

    if (mapEnd == fromSeqNum)
        return 0;

    //RexmitQueue::const_iterator i = rexmitQueue.begin();
    uint32_t bytes = 0;

    //while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
    //    i++;
    auto iter = rexmitMap.upper_bound(fromSeqNum);
    while (iter != rexmitMap.end() && (iter->second.sacked || iter->second.rexmitted)) {
        bytes += (iter->second.endSeqNum - iter->second.beginSeqNum);
        iter++;
    }

    return bytes;

//    if(m_sackedOut > 0 || m_retrans > 0){
//        return m_sackedOut + m_retrans;
//    }
}

void TcpSackRexmitQueue::resetSackedBit()
{
    if (!m_updatedSackEnabled) {
        for (auto& elem : rexmitQueue)
            elem.sacked = false;
        return;
    }

//    for (auto& elem : rexmitMap){
//        if(elem.second.sacked){
//            m_sackedOut -= elem.second.endSeqNum - elem.second.beginSeqNum;
//            elem.second.sacked = false; // reset sacked bit
//        }
////        if(!elem.second.lost){
////            m_lostOut += elem.second.endSeqNum - elem.second.beginSeqNum;
////            elem.second.lost = true; // reset rexmitted bit
////        }
//    }
}

void TcpSackRexmitQueue::resetRexmittedBit()
{
    if (!m_updatedSackEnabled) {
        for (auto& elem : rexmitQueue)
            elem.rexmitted = false;
        return;
    }

//    for (auto& elem : rexmitMap){
//        if(elem.second.rexmitted){
//            m_retrans -= elem.second.endSeqNum - elem.second.beginSeqNum;
//            elem.second.rexmitted = false; // reset rexmitted bit
//        }
//    }
}

uint32_t TcpSackRexmitQueue::getTotalAmountOfSackedBytes() //const
{
    if (!m_updatedSackEnabled) {
        uint32_t bytes = 0;

        for (const auto& elem : rexmitQueue) {
            if (elem.sacked)
                bytes += (elem.endSeqNum - elem.beginSeqNum);
        }

        return bytes;
    }

    //uint32_t bytes = 0;
    //uint32_t sackedOut = 0;
//    //uint32_t retrans = 0;
//
////    for (const auto& elem : rexmitQueue) {
////        if (elem.sacked)
////            bytes += (elem.endSeqNum - elem.beginSeqNum);
////    }
//    for (const auto& elem : rexmitMap) {
//        if (elem.second.sacked){
//            bytes += (elem.first - elem.second.beginSeqNum);
////            sackedOut += (elem.first - elem.second.beginSeqNum);
//        }
////        else if (elem.second.rexmitted){
////            retrans += (elem.first - elem.second.beginSeqNum);
////        }
//    }

//    m_sackedOut = sackedOut;
//    m_retrans = retrans;

    //std::cout << "\n m_sackedOut " << m_sackedOut << endl;
    return m_sackedOut;
}

uint32_t TcpSackRexmitQueue::getAmountOfSackedBytes(uint32_t fromSeqNum)// const //NOT NEEDED
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

        uint32_t bytes = 0;
        RexmitQueue::const_reverse_iterator i = rexmitQueue.rbegin();

        for (; i != rexmitQueue.rend() && seqLE(fromSeqNum, i->beginSeqNum); i++) {
            if (i->sacked)
                bytes += (i->endSeqNum - i->beginSeqNum);
        }

        if (i != rexmitQueue.rend()
            && seqLess(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum) && i->sacked)
        {
            bytes += (i->endSeqNum - fromSeqNum);
        }

        return bytes;
    }

    //ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

    uint32_t bytes = 0;
//    auto imap = rexmitMap.rbegin();
//
//    std::cout << "\n rend  beginSeqNum: " << rexmitMap.rend()->second.beginSeqNum << endl;
//    for (; imap != rexmitMap.rend()++ && seqLE(fromSeqNum, imap->second.beginSeqNum); imap++) {
//        if (imap->second.sacked){
//            bytes += (imap->first - imap->second.beginSeqNum);
//        }
//    }
//    if (i != rexmitQueue.rend()
//        && seqLess(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum) && i->sacked)
//    {
//        bytes += (i->endSeqNum - fromSeqNum);
//    }
    auto iter = rexmitMap.upper_bound(fromSeqNum);

    while (iter != rexmitMap.end()) {
        if (iter->second.sacked){
            bytes += (iter->first - iter->second.beginSeqNum);
        }

        if(bytes >= (1448*3)){
            break;
        }
        iter++;
    }

    return bytes;
}

bool TcpSackRexmitQueue::checkIsLost(uint32_t seqNo, uint32_t highestSack)
{
    if (!m_updatedSackEnabled) {
        if (seqNo >= highestSack)
            return false;

        const auto *state = conn == nullptr ? nullptr : conn->getState();
        if (state == nullptr)
            return false;

        return getNumOfDiscontiguousSacks(seqNo) >= state->dupthresh ||
               getAmountOfSackedBytes(seqNo) >= state->dupthresh * state->snd_mss;
    }

    if(findRegion(seqNo)){
        if (seqNo >= highestSack)
        {
            return false;
        }
    //
        // In theory, using a map and hints when inserting elements can improve
        // performance
    //    for (auto it = rexmitMap.begin(); it != rexmitMap.end(); ++it)
    //    {
    //        // Search for the right iterator before calling IsLost()
    //        if (seqNo < it->second.endSeqNum)
    //        {
    //            if (it->second.lost)
    //            {
    //                return true;
    //            }
    //
    //            if (it->second.sacked)
    //            {
    //                return false;
    //            }
    //        }
    //    }
        return rexmitMap.at(seqNo).lost;
    }
    return false;
    //return false;
}

bool TcpSackRexmitQueue::checkHeadIsLost()
{
    if (!m_updatedSackEnabled || rexmitMap.empty())
        return false;

    return rexmitMap.begin()->second.lost;
}

bool TcpSackRexmitQueue::isRetransmitted(uint32_t seqNo)
{
    if (!m_updatedSackEnabled) {
        if (end == seqNo)
            return false;

        for (const auto& elem : rexmitQueue) {
            if (seqLE(elem.beginSeqNum, seqNo) && seqLess(seqNo, elem.endSeqNum))
                return elem.rexmitted;
        }
        return false;
    }

    if(findRegion(seqNo)){
       auto iter = rexmitMap.at(seqNo);
       return iter.rexmitted;
    }
    return false;
}

uint32_t TcpSackRexmitQueue::getLost()
{
    if (!m_updatedSackEnabled)
        return 0;

    return m_lostOut;
}

bool TcpSackRexmitQueue::updateLost(uint32_t highestSackedSeqNum)
{
    if (!m_updatedSackEnabled)
        return false;

    bool itemLost = false;
    uint32_t sacked = 0;
    auto iter = rexmitMap.upper_bound(highestSackedSeqNum);
    std::map<uint32_t, Region>::reverse_iterator rit(iter);
    for (; rit != rexmitMap.rend(); rit++) {
        Region& region = rit->second;
        if (region.sacked){
            sacked++;
        }

        if(sacked >= 3)
        {
            if (!region.sacked && markRegionLost(region, true)) {
                itemLost = true;
//                if(rit->second.rexmitted == true){
//                    rit->second.rexmitted = false;
//                    m_retrans -= rit->second.endSeqNum - rit->second.beginSeqNum;
//                }
            }
        }
    }

    if(sacked >= 3)
    {
        auto item = rexmitMap.begin();
        Region& region = item->second;
        if (markRegionLost(region, true)) {
            itemLost = true;
        }
    }

    consistencyCheck();
    return itemLost;
}

void TcpSackRexmitQueue::markHeadAsLost()
{
    if (!m_updatedSackEnabled)
        return;

    if (!rexmitMap.empty())
    {
        // If the head is sacked (reneging by the receiver the previously sent
        // information) we revert the sacked flag.
        // A sacked head means that we should advance SND.UNA.. so it's an error.
        if (rexmitMap.begin()->second.sacked)
        {
            std::cout << "\n SACKED HEAD ERROR AT " << simTime() << endl;
            rexmitMap.begin()->second.sacked = false;
            m_sackedOut -= rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }

        if (rexmitMap.begin()->second.rexmitted)
        {
            rexmitMap.begin()->second.rexmitted = false;
            m_retrans -= rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }

        markRegionLost(rexmitMap.begin()->second, false);
    }
}

void TcpSackRexmitQueue::markHeadAsLostIfUnsacked()
{
    if (!m_updatedSackEnabled || rexmitMap.empty())
        return;

    Region& head = rexmitMap.begin()->second;
    if (head.sacked)
        return;

    if (head.rexmitted)
    {
        head.rexmitted = false;
        m_retrans -= head.endSeqNum - head.beginSeqNum;
    }

    markRegionLost(head, false);
}

void TcpSackRexmitQueue::setAllLost()
{
    if (!m_updatedSackEnabled) {
        resetSackedBit();
        resetRexmittedBit();
        return;
    }

    // From RFC 6675, Section 5.1
    // [RFC2018] suggests that a TCP sender SHOULD expunge the SACK
    // information gathered from a receiver upon a retransmission timeout
    // (RTO) "since the timeout might indicate that the data receiver has
    // reneged."  Additionally, a TCP sender MUST "ignore prior SACK
    // information in determining which data to retransmit."
    // It has been suggested that, as long as robust tests for
    // reneging are present, an implementation can retain and use SACK
    // information across a timeout event [Errata1610].
    // The head of the sent list will not be marked as sacked, therefore
    // will be retransmitted, if the receiver renegotiate the SACK blocks
    // that we received.
//    m_sackedOut = 0;
//    m_lostOut = m_sentSize;
//    for (auto iter = rexmitMap.begin(); iter != rexmitMap.end(); iter++) {
//        Region& region = iter->second;
//        region.sacked = false;
//        region.lost = true;
//    }

    m_retrans = 0;
    m_lostOut = 0;

    for (auto iter = rexmitMap.begin(); iter != rexmitMap.end(); iter++) {
        Region& region = iter->second;
        if(region.lost)
        {
            m_lostOut += region.endSeqNum - region.beginSeqNum;
        }
        else if (!region.sacked)
            markRegionLost(region, false);
        region.rexmitted = false;
    }
    consistencyCheck();
}

uint32_t TcpSackRexmitQueue::getNumOfDiscontiguousSacks(uint32_t fromSeqNum)//const NOT NEEDED
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

        if (rexmitQueue.empty() || (fromSeqNum == end))
            return 0;

        RexmitQueue::const_iterator i = rexmitQueue.begin();
        uint32_t counter = 0;

        while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
            i++;

        bool prevSacked = false;

        while (i != rexmitQueue.end()) {
            if (i->sacked && !prevSacked)
                counter++;

            prevSacked = i->sacked;
            i++;
        }

        return counter;
    }

    //ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

    if (rexmitMap.empty() || (fromSeqNum == end))
        return 0;

    //RexmitQueue::const_iterator i = rexmitQueue.begin();
    uint32_t counter = 0;
//
//    while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum)) // search for seqNum
//        i++;
    auto iter = rexmitMap.upper_bound(fromSeqNum);
    // search for discontiguous sacked regions
    bool prevSacked = false;

    while (iter != rexmitMap.end()) {
        if (iter->second.sacked && !prevSacked){
            counter++;
        }
        if(counter >= 3){
            //std::cout << "\n COUNTER: " << counter << endl;
            if (!iter->second.sacked)
                markRegionLost(iter->second, false);
            //break;
        }
        prevSacked = iter->second.sacked;
        iter++;
    }

    if (counter >= 3)
    {
        auto iter = rexmitMap.begin();
        markRegionLost(iter->second, false);
    }
    //std::cout << "\n COUNT TEST: " << countTest << endl;
    //std::cout << "\n" << detailedInfo() << endl;


//    while (i != rexmitQueue.end()) {
//        if (i->sacked && !prevSacked)
//            counter++;
//
//        prevSacked = i->sacked;
//        i++;
//    }
    return counter;
}

void TcpSackRexmitQueue::checkSackBlock(uint32_t fromSeqNum, uint32_t& length, bool& sacked, bool& rexmitted) //const
{
    if (!m_updatedSackEnabled) {
        ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));

        RexmitQueue::const_iterator i = rexmitQueue.begin();

        while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
            i++;

        ASSERT(i != rexmitQueue.end());
        ASSERT(seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

        length = (i->endSeqNum - fromSeqNum);
        sacked = i->sacked;
        rexmitted = i->rexmitted;
        return;
    }

    auto iter = length == 0 ? rexmitMap.upper_bound(fromSeqNum) : rexmitMap.find(fromSeqNum + length);
    if (iter == rexmitMap.end())
        iter = rexmitMap.upper_bound(fromSeqNum);

    ASSERT(iter != rexmitMap.end());
    ASSERT(seqLE(iter->second.beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, iter->second.endSeqNum));

    length = (iter->second.endSeqNum - fromSeqNum);
    sacked = iter->second.sacked;
    rexmitted = iter->second.rexmitted;
}

void TcpSackRexmitQueue::checkSackBlockLost(uint32_t fromSeqNum, uint32_t& length, bool& sacked, bool& rexmitted, bool&lost) //const
{
    if (!m_updatedSackEnabled) {
        checkSackBlock(fromSeqNum, length, sacked, rexmitted);
        lost = checkIsLost(fromSeqNum, getHighestSackedSeqNum());
        return;
    }

    //ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));
    auto iter = rexmitMap.find(fromSeqNum + length);
    if (iter == rexmitMap.end())
        iter = rexmitMap.upper_bound(fromSeqNum);

    ASSERT(iter != rexmitMap.end());

    length = (iter->second.endSeqNum - iter->second.beginSeqNum);
    sacked = iter->second.sacked;
    rexmitted = iter->second.rexmitted;
    lost = iter->second.lost;
}

std::map<uint32_t, TcpSackRexmitQueue::Region>::iterator TcpSackRexmitQueue::searchSackBlock(uint32_t fromSeqNum) //const
{
    //ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));

    return rexmitMap.upper_bound(fromSeqNum);
}

uint32_t TcpSackRexmitQueue::getInFlight()
{
    if (!m_updatedSackEnabled) {
        const auto *state = conn == nullptr ? nullptr : conn->getState();
        return state == nullptr ? 0 : state->snd_max - state->snd_una;
    }

    return m_sentSize - (m_sackedOut + m_lostOut) + m_retrans;
    //return m_sentSizel
}

void TcpSackRexmitQueue::clearRecentLossSample()
{
    m_recentLossSampleValid = false;
    m_recentLossBytes = 0;
    m_recentLossTxInFlight = 0;
    m_recentLossIsAppLimited = false;
}

bool TcpSackRexmitQueue::getRecentLossSample(uint32_t& txInFlight, uint32_t& lostBytes, bool& isAppLimited) const
{
    if (!m_recentLossSampleValid)
        return false;

    txInFlight = m_recentLossTxInFlight;
    lostBytes = m_recentLossBytes;
    isAppLimited = m_recentLossIsAppLimited;
    return true;
}

void TcpSackRexmitQueue::noteLostRegion(const Region& region)
{
    const uint32_t lostBytes = region.endSeqNum - region.beginSeqNum;
    if (!m_recentLossSampleValid) {
        m_recentLossSampleValid = true;
        m_recentLossBytes = lostBytes;
        m_recentLossTxInFlight = region.m_txInFlight;
        m_recentLossIsAppLimited = region.m_isAppLimited;
    }
    else {
        m_recentLossBytes += lostBytes;
        m_recentLossTxInFlight = std::max(m_recentLossTxInFlight, region.m_txInFlight);
        m_recentLossIsAppLimited = m_recentLossIsAppLimited && region.m_isAppLimited;
    }
}

bool TcpSackRexmitQueue::markRegionLost(Region& region, bool recordRecentLossSample)
{
    if (region.lost)
        return false;

    region.lost = true;
    const uint32_t lostBytes = region.endSeqNum - region.beginSeqNum;
    m_lostOut += lostBytes;
    m_totalDetectedLostBytes += lostBytes;
    if (recordRecentLossSample)
        noteLostRegion(region);
    return true;
}

bool TcpSackRexmitQueue::markRegionLostByEndSeq(uint32_t endSeq, bool recordRecentLossSample)
{
    if (!m_updatedSackEnabled)
        return false;

    auto it = rexmitMap.find(endSeq);
    return it != rexmitMap.end() && markRegionLost(it->second, recordRecentLossSample);
}

void TcpSackRexmitQueue::skbSent(uint32_t seqNum, simtime_t m_firstSentTime, simtime_t m_lastSentTime, simtime_t m_deliveredTime, uint32_t m_txInFlight, bool m_isAppLimited, uint32_t m_delivered, uint32_t m_appLimited)
{
    if (!m_updatedSackEnabled)
        return;

    inet::tcp::TcpSackRexmitQueue::Region& region = rexmitMap.at(seqNum);
    region.m_firstSentTime = m_firstSentTime;
    region.m_lastSentTime = m_lastSentTime;
    region.m_deliveredTime = m_deliveredTime;
    region.m_txInFlight = m_txInFlight;
    region.m_isAppLimited = (m_appLimited != 0);
    region.m_delivered = m_delivered;
    //return m_sentSizel
}

TcpSackRexmitQueue::Region& TcpSackRexmitQueue::getRegion(uint32_t seqNum)
{
    return rexmitMap.at(seqNum);
}

bool TcpSackRexmitQueue::findRegion(uint32_t seqNum)
{
    if (!m_updatedSackEnabled)
        return false;

    return !(rexmitMap.find(seqNum) == rexmitMap.end());
}

void TcpSackRexmitQueue::consistencyCheck() const
{
    if (!m_updatedSackEnabled)
        return;

    uint32_t sacked = 0;
    uint32_t lost = 0;
    uint32_t retrans = 0;

    for (auto it = rexmitMap.begin(); it != rexmitMap.end(); ++it)
    {
        const uint32_t packetSize = it->second.endSeqNum - it->second.beginSeqNum;
        if (it->second.sacked)
        {
            sacked += packetSize;
        }
        if (it->second.lost)
        {
            lost += packetSize;
        }
        if (it->second.rexmitted)
        {
            retrans += packetSize;
        }
    }

    if(sacked != m_sackedOut){
        std::cout << "\n SACKED IS NOT THE SAME" << endl;
        std::cout << "\n CALCULATED SACK: " << sacked << endl;
        std::cout << "\n CURRENT SACK: " << m_sackedOut << endl;
    }

    if(lost != m_lostOut){
        std::cout << "\n LOST IS NOT THE SAME" << endl;
        std::cout << "\n CALCULATED LOST: " << lost << endl;
        std::cout << "\n CURRENT LOST: " << m_lostOut << endl;
    }

    if(retrans != m_retrans){
        std::cout << "\n RETRANS IS NOT THE SAME" << endl;
        std::cout << "\n CALCULATED RETRANS: " << retrans << endl;
        std::cout << "\n CURRENT RETRANS: " << m_retrans << endl;
    }
}

uint32_t TcpSackRexmitQueue::getTailSequence()
{
    return m_updatedSackEnabled && !rexmitMap.empty() ? rexmitMap.begin()->second.endSeqNum : end;
}

bool TcpSackRexmitQueue::checkRackLoss(TcpRack* rack, double &timeout)
{
    if (!m_updatedSackEnabled || rack == nullptr)
        return false;

    bool markedLost = false;
    for (auto it = rexmitMap.begin(); it != rexmitMap.end(); ++it)
    {
        Region& region = it->second;
        if (region.sacked)
        {
            continue;
        }

        if (region.lost && !region.rexmitted)
        {
            continue;
        }

        else if (!rack->sentAfter(rack->getXmitTs(), region.m_lastSentTime, rack->getEndSeq(), region.endSeqNum))
        {
            break;
        }

        double remaining = region.m_lastSentTime.dbl() + rack->getRtt().dbl() + rack->getReoWnd() - simTime().dbl();
        if (remaining <= 0)
        {
            if (markRegionLost(region, true))
            {
                markedLost = true;
            }
            // Marking the retransmitted packets that are lost again
            else if (region.rexmitted)
            {
                noteLostRegion(region);
                region.rexmitted = false;
                m_retrans -= region.endSeqNum - region.beginSeqNum;
                markedLost = true;
            }
        }
        else
        {
            timeout = std::max(remaining, timeout);
        }
    }
    return markedLost;
}

bool TcpSackRexmitQueue::isRetransmittedDataAcked(uint32_t seqNum)
{
    if (!m_updatedSackEnabled || rexmitMap.find(seqNum) == rexmitMap.end())
        return false;

    TcpSackRexmitQueue::Region& region = rexmitMap.at(seqNum);
    return !region.sacked && region.rexmitted;
}

} // namespace tcp

} // namespace inet
