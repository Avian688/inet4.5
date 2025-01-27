//
// Copyright (C) 2009-2010 Thomas Reschka
// Copyright (C) 2011 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/transportlayer/tcp/TcpSackRexmitQueue.h"
#include <chrono>

namespace inet {

namespace tcp {

TcpSackRexmitQueue::TcpSackRexmitQueue()
{
    conn = nullptr;
    begin = end = 0;
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
}

std::string TcpSackRexmitQueue::str() const
{
    std::stringstream out;

    out << "[" << begin << ".." << end << ")";
    return out.str();
}

std::string TcpSackRexmitQueue::detailedInfo() const
{
//    std::stringstream out;
//    out << str() << endl;
//
//    uint j = 1;
//
//    for (const auto& elem : rexmitQueue) {
//        out << j << ". region: [" << elem.beginSeqNum << ".." << elem.endSeqNum
//            << ") \t sacked=" << elem.sacked << "\t rexmitted=" << elem.rexmitted
//            << endl;
//        j++;
//    }
//    return out.str();
    std::stringstream out;
    out << str() << endl;

    uint j = 1;

    for (auto elem : rexmitMap) {
        out << j << ". region: [" << elem.second.beginSeqNum << ".." << elem.second.endSeqNum
            << ") \t sacked=" << elem.second.sacked << "\t rexmitted=" << elem.second.rexmitted
            << endl;
        j++;
    }
    return out.str();
}

void TcpSackRexmitQueue::discardUpTo(uint32_t seqNum)
{
    auto start = std::chrono::high_resolution_clock::now();
//    if(!(seqLE(begin, seqNum) && seqLE(seqNum, end))){
//        std::cout << "\n ASSERT in discardUpTo FAILED" << endl;
//        std::cout << "\n" << detailedInfo() << endl;
//    }
    ASSERT(seqLE(begin, seqNum) && seqLE(seqNum, end));
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
            auto iEnd = rexmitMap.end();
//            while ((i != rexmitMap.end()) && seqLE(i->second.endSeqNum, seqNum))
//            {
//                rexmitMap.erase(i->second.endSeqNum);
//                i++;
//            }
            //std::cout << "\n DISCARDING UP TO: " << seqNum << endl;
            while ((i != iEnd) && seqLE(i->second.endSeqNum, seqNum)){
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
            if (i != iEnd) {
                //ASSERT(seqLE(i->second.beginSeqNum, seqNum) && seqLess(seqNum, i->second.endSeqNum));
                m_sentSize -= seqNum - i->second.beginSeqNum;
                if(i->second.sacked){
                    m_sackedOut -= seqNum - i->second.beginSeqNum;
                }

                if(i->second.rexmitted){
                    m_retrans -= seqNum - i->second.beginSeqNum;
                }

                if(i->second.lost){
                    m_lostOut -= seqNum - i->second.beginSeqNum;
                }

                i->second.beginSeqNum = seqNum;
            }
        }
    begin = seqNum;

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    discardUpToTime += std::chrono::duration<double>(elapsed).count();

    // TESTING queue:
    //ASSERT(checkQueue());
}

void TcpSackRexmitQueue::enqueueSentData(uint32_t fromSeqNum, uint32_t toSeqNum)
{
    auto start = std::chrono::high_resolution_clock::now();
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

    bool found = false;
    Region region;

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
            //std::cout << "\n MARKING AS RETRANSMITTED AT SIMTIME: " << simTime() << endl;
            iter->second.rexmitted = true;
            fromSeqNum = iter->second.endSeqNum;
            found = true;
            iter++;
        }

        if (fromSeqNum != toSeqNum) { //rarely called? this is called if the block does not exist AND fromSeqNum is not the end of the current queue
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

    begin = rexmitMap.begin()->second.beginSeqNum;
    end = (--rexmitMap.end())->second.endSeqNum;
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    enqueueSentDataTime += std::chrono::duration<double>(elapsed).count();
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
    auto start = std::chrono::high_resolution_clock::now();

    if (seqLess(fromSeqNum, begin))
        fromSeqNum = begin;

    ASSERT(seqLess(fromSeqNum, end));
    ASSERT(seqLess(begin, toSeqNum) && seqLE(toSeqNum, end));
    ASSERT(seqLess(fromSeqNum, toSeqNum));

    bool found = false;

    if (!rexmitMap.empty()) {
        //auto i = rexmitQueue.begin();

        //while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
        //    i++;
        auto iter = rexmitMap.upper_bound(fromSeqNum);

        ASSERT(iter != rexmitMap.end() && seqLE(iter->second.beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, iter->second.endSeqNum));
        //ASSERT(i != rexmitQueue.end() && seqLE(i->beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, i->endSeqNum));

        //std::cout << "\n SET SACKET BIT" << endl;
        //std::cout << "\n fromSeqNum " << fromSeqNum << endl;
        //std::cout << "\n toSeqNum " << toSeqNum << endl;
        //std::cout << "\n i->beginSeqNum " << i->beginSeqNum << endl;
        //std::cout << "\n i->endSeqNum " << i->endSeqNum << endl;
        bool firstTest = seqGreater(fromSeqNum, iter->second.beginSeqNum);
        bool secondTest = seqLess(toSeqNum, iter->second.endSeqNum);
        if (iter->second.beginSeqNum != fromSeqNum) {
//            if(seqGreater(fromSeqNum, iter->second.beginSeqNum) && seqLE(toSeqNum, iter->second.endSeqNum)){ //Do not forget case where it is last block (toSeqNum == i->endSeqNum)
//                //std::cout << "\n FOUND BLOCK TO BE SACKED, WITHIN LARGER BLOCK" << endl;
//                uint32_t oldRegionEnd = iter->second.endSeqNum;
//
////                std::cout << "\n BEFORE ADD: " << endl;
////                for(auto it = rexmitMap.cbegin(); it != rexmitMap.cend(); ++it)
////                {
////                    std::cout << "KEY: "<< it->first << "\n";
////                    std::cout << "beginSeqNum" << it->second.beginSeqNum << "\n";
////                    std::cout << "endSeqNum" << it->second.endSeqNum << "\n";
////                    std::cout << "sacked" << it->second.sacked << "\n";
////                    std::cout << "rexmitted" << it->second.rexmitted << "\n";
////                }
//
//                bool rex = false;
//                bool los = false;
//                Region startRegion;
//                startRegion.beginSeqNum = iter->second.beginSeqNum;
//                startRegion.endSeqNum = fromSeqNum;
//                startRegion.sacked = iter->second.sacked;
//                startRegion.rexmitted = iter->second.rexmitted;
//                rex = iter->second.rexmitted;
//                startRegion.lost = iter->second.lost;
//                los = iter->second.lost;
//
//                rexmitMap.erase(iter->second.endSeqNum);
//                //i->endSeqNum = fromSeqNum;
//
//                rexmitMap.insert({fromSeqNum, startRegion}); //potential merging
//
//                Region region;
//                region.beginSeqNum = fromSeqNum;
//                region.endSeqNum = toSeqNum;
//                if(!startRegion.sacked){
//                    m_sackedOut += toSeqNum - fromSeqNum;
//                }
//                region.sacked = true;
//                region.rexmitted = false;
//                region.lost = false;
//                //iter++;                                                                     //TODO Fix this error
//                //rexmitQueue.insert(i,region);
//                rexmitMap.insert({toSeqNum, region});
//
//                Region endRegion;
//                endRegion.beginSeqNum = toSeqNum;
//                endRegion.endSeqNum = oldRegionEnd;
//                endRegion.sacked = startRegion.sacked;
//                endRegion.rexmitted = startRegion.rexmitted;
//                endRegion.lost = startRegion.lost;
//                //rexmitQueue.insert(i,endRegion);
//                rexmitMap.insert({oldRegionEnd, endRegion});
//
//                //get start seqNum of large block
//                //get end seqNum of large black
//                //
//            }
//            else{
                //Gap in queue (should rarely if at all occur)
            Region region = iter->second;
            region.endSeqNum = fromSeqNum;
            //rexmitQueue.insert(i, region);
            rexmitMap.insert({region.endSeqNum, region}); //TODO potentially fix
            iter->second.beginSeqNum = fromSeqNum;
            //}
        }

        iter = rexmitMap.upper_bound(fromSeqNum); //sets sacked bit if the following regions of the above check are also within the range
        while ((iter != rexmitMap.end() && seqLE(iter->second.endSeqNum, toSeqNum))) {  //Most common case. Should split existing large region if it exists into smaller region
            if (seqGE(iter->second.beginSeqNum, fromSeqNum)) { // Search region in queue!
                found = true;
                if(!iter->second.sacked){
                    m_sackedOut += iter->second.endSeqNum - iter->second.beginSeqNum;
                }

                iter->second.sacked = true; // set sacked bit //may need to merge

                if(iter->second.lost == true){
                    m_lostOut -= iter->second.endSeqNum - iter->second.beginSeqNum;
                    iter->second.lost = false;
                }
                //iter->second.lost = false; CHANGEDRECNTLY
                //std::cout << "\n Found block, setting sacked bit!" << endl;
                //std::cout << "\n"<<  detailedInfo() << endl;
            }
//            std::cout << "\n iter->second.beginSeqNum: " << iter->second.beginSeqNum << endl;
//            std::cout << "\n iter->second.endSeqNum: " << iter->second.endSeqNum << endl;
//            std::cout << "\n fromSeqNum: " << fromSeqNum << endl;
//            std::cout << "\n toSeqNum: " << toSeqNum << endl;
//            std::cout << "\n"<<  detailedInfo() << endl;
            iter++;
        }
        //splits last element
        if (iter != rexmitMap.end() && seqLess(iter->second.beginSeqNum, toSeqNum) && seqLess(toSeqNum, iter->second.endSeqNum)) { //Edge case, rarely happens ignore for now
            Region region = iter->second;
            //std::cout << "\n ALTERNATE Found block, setting sacked bit!" << endl;
            region.endSeqNum = toSeqNum;
            if(!region.sacked){
                m_sackedOut += iter->second.endSeqNum - iter->second.beginSeqNum;
            }
            std::cout << "\n EDGE CASE BEING CALLED" << endl;
            //iter->second.sacked = true; // set sacked bit //may need to merge
            region.sacked = true;
            region.lost = false;
            //rexmitQueue.insert(i, region);
            rexmitMap.insert({region.endSeqNum, region});
            iter->second.beginSeqNum = toSeqNum; //TODO MAYBE FIX
        }
    }

    if (!found)
        EV_DETAIL << "FAILED to set sacked bit for region: [" << fromSeqNum << ".." << toSeqNum << "). Not found in retransmission queue.\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    setSackedBitTime += std::chrono::duration<double>(elapsed).count();

    //ASSERT(checkQueue());
}

bool TcpSackRexmitQueue::getSackedBit(uint32_t seqNum) //const
{
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT(seqLE(begin, seqNum) && seqLE(seqNum, end));

    //RexmitQueue::const_iterator i = rexmitQueue.begin();

    if (end == seqNum){
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = end - start;
        getSackedBitTime += std::chrono::duration<double>(elapsed).count();
        return false;
    }
    auto iter = rexmitMap.upper_bound(seqNum);
//    while (i != rexmitQueue.end() && seqLE(i->endSeqNum, seqNum))
//        i++;

    ASSERT((iter != rexmitMap.end()) && seqLE(iter->second.beginSeqNum, seqNum) && seqLess(seqNum, iter->second.endSeqNum));

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getSackedBitTime += std::chrono::duration<double>(elapsed).count();

    return iter->second.sacked;
}

uint32_t TcpSackRexmitQueue::getHighestSackedSeqNum() //const
{
    auto start = std::chrono::high_resolution_clock::now();
    for (auto iter = rexmitMap.rbegin(); iter != rexmitMap.rend(); iter++) {
        if (iter->second.sacked){
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end - start;
            getHighestSackedSeqNumTime += std::chrono::duration<double>(elapsed).count();
            return iter->second.endSeqNum;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getHighestSackedSeqNumTime += std::chrono::duration<double>(elapsed).count();

    return begin;
}

uint32_t TcpSackRexmitQueue::getHighestRexmittedSeqNum() //const
{
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

    auto start = std::chrono::high_resolution_clock::now();
    for (auto iter = rexmitMap.rbegin(); iter != rexmitMap.rend(); iter++) {
        //std::cout << "\n In getHighestRexmittedSeqNum() " << "\n";
        if (iter->second.rexmitted){
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end - start;
            getHighestSackedSeqNumTime += std::chrono::duration<double>(elapsed).count();
            return iter->second.endSeqNum;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getHighestRexmittedSeqNumTime += std::chrono::duration<double>(elapsed).count();
    return begin;
}

uint32_t TcpSackRexmitQueue::checkRexmitQueueForSackedOrRexmittedSegments(uint32_t fromSeqNum) //const
{
    auto start = std::chrono::high_resolution_clock::now();

    ASSERT(seqLE(begin, fromSeqNum) && seqLE(fromSeqNum, end));

    if (rexmitMap.empty() || (end == fromSeqNum))
        return 0;

    //RexmitQueue::const_iterator i = rexmitQueue.begin();
    uint32_t bytes = 0;

    //while (i != rexmitQueue.end() && seqLE(i->endSeqNum, fromSeqNum))
    //    i++;
    auto iter = rexmitMap.upper_bound(fromSeqNum);
    while (iter != rexmitMap.end() && ((iter->second.sacked || iter->second.rexmitted))) {
        ASSERT(seqLE(iter->second.beginSeqNum, fromSeqNum) && seqLess(fromSeqNum, iter->second.endSeqNum));

        bytes += (iter->second.endSeqNum - fromSeqNum);
        fromSeqNum = iter->second.endSeqNum;
        iter++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getHighestRexmittedSeqNumTime += std::chrono::duration<double>(elapsed).count();

    return bytes;
}

void TcpSackRexmitQueue::resetSackedBit()
{
    for (auto& elem : rexmitMap){
        if(elem.second.sacked){
            m_sackedOut -= elem.second.endSeqNum - elem.second.beginSeqNum;
            elem.second.sacked = false; // reset sacked bit
        }

    }
}

void TcpSackRexmitQueue::resetRexmittedBit()
{
    for (auto& elem : rexmitMap){
        if(elem.second.rexmitted){
            m_retrans -= elem.second.endSeqNum - elem.second.beginSeqNum;
            elem.second.rexmitted = false; // reset rexmitted bit
        }
    }

}

uint32_t TcpSackRexmitQueue::getTotalAmountOfSackedBytes() //const
{
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t bytes = 0;
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

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getTotalAmountOfSackedBytesTime += std::chrono::duration<double>(elapsed).count();
//    m_sackedOut = sackedOut;
//    m_retrans = retrans;

    //std::cout << "\n m_sackedOut " << m_sackedOut << endl;
    return m_sackedOut;
}

uint32_t TcpSackRexmitQueue::getAmountOfSackedBytes(uint32_t fromSeqNum)// const
{
    auto start = std::chrono::high_resolution_clock::now();
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

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getAmountOfSackedBytesTime += std::chrono::duration<double>(elapsed).count();
    return bytes;
}

bool TcpSackRexmitQueue::checkIsLost(uint32_t seqNo, uint32_t highestSack)
{
    if (seqNo >= highestSack)
    {
        return false;
    }
//
//    // In theory, using a map and hints when inserting elements can improve
//    // performance
//    for (auto it = rexmitMap.begin(); it != rexmitMap.end(); ++it)
//    {
//        // Search for the right iterator before calling IsLost()
//        if (it->second.beginSeqNum <= seqNo && seqNo < it->second.beginSeqNum + 1448)
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
//
//    return false;
    return rexmitMap.upper_bound(seqNo)->second.lost;
}

bool TcpSackRexmitQueue::checkHeadIsLost()
{
    return rexmitMap.begin()->second.lost;
}

std::tuple<bool, bool> TcpSackRexmitQueue::getLostAndRetransmitted(uint32_t seqNo)
{
    const auto entry = rexmitMap.upper_bound(seqNo)->second;

    return {entry.lost, entry.rexmitted};
}

uint32_t TcpSackRexmitQueue::getLost()
{
    return m_lostOut;
}

void TcpSackRexmitQueue::updateLost(uint32_t highestSackedSeqNum)
{
    uint32_t sacked = 0;
    auto iter = rexmitMap.upper_bound(highestSackedSeqNum);
    std::map<uint32_t, Region>::reverse_iterator rit(iter);
    for (rit; rit != rexmitMap.rend(); rit++) {
        if (rit->second.sacked){
            sacked++;
        }

        if(sacked >= 3)
        {
            if(rit->second.sacked == false && rit->second.lost == false){
                rit->second.lost = true;
                m_lostOut += rit->second.endSeqNum - rit->second.beginSeqNum;
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
        if(item->second.lost == false){
            rit->second.lost = true;
            m_lostOut += rit->second.endSeqNum - rit->second.beginSeqNum;
//            if(rit->second.rexmitted == true){
//                rit->second.rexmitted = false;
//                m_retrans -= rit->second.endSeqNum - rit->second.beginSeqNum;
//            }
        }
    }

//    if(iter->second.sacked == false && iter->second.lost == false){
//        iter->second.l = true;
//    }
}

void TcpSackRexmitQueue::markHeadAsLost()
{
    if (!rexmitMap.empty())
    {
        // If the head is sacked (reneging by the receiver the previously sent
        // information) we revert the sacked flag.
        // A sacked head means that we should advance SND.UNA.. so it's an error.
        if (rexmitMap.begin()->second.sacked)
        {
            rexmitMap.begin()->second.sacked = false;
            m_sackedOut -= rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }

        if (rexmitMap.begin()->second.rexmitted)
        {
            rexmitMap.begin()->second.rexmitted = false;
            m_retrans -= rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }

        if (!rexmitMap.begin()->second.lost)
        {
            rexmitMap.begin()->second.lost = true;
            m_lostOut +=rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }
    }
}

void TcpSackRexmitQueue::setAllLost()
{
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
    m_sackedOut = 0;
    m_lostOut = m_sentSize;
    for (auto iter = rexmitMap.begin(); iter != rexmitMap.end(); iter++) {
        iter->second.sacked = false;
        iter->second.lost = true;
    }
}

uint32_t TcpSackRexmitQueue::getNumOfDiscontiguousSacks(uint32_t fromSeqNum)//const
{
    auto start = std::chrono::high_resolution_clock::now();
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
            if (!iter->second.sacked && !iter->second.lost)
            {
                iter->second.lost = true;
                m_lostOut += iter->second.endSeqNum - iter->second.beginSeqNum;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end - start;
            getNumOfDiscontiguousSacksTime += std::chrono::duration<double>(elapsed).count();
            //break;
        }
        prevSacked = iter->second.sacked;
        iter++;
    }

    if (counter >= 3)
    {
        auto iter = rexmitMap.begin();
        if (!iter->second.lost)
        {
            iter->second.lost = true;
            m_lostOut += iter->second.endSeqNum - iter->second.beginSeqNum;
        }
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
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    getNumOfDiscontiguousSacksTime += std::chrono::duration<double>(elapsed).count();
    return counter;
}

void TcpSackRexmitQueue::checkSackBlockIter(uint32_t fromSeqNum, uint32_t& length, bool& sacked, bool& rexmitted, std::map<uint32_t, Region>::iterator& iter) //const
{
    auto start = std::chrono::high_resolution_clock::now();
    //ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));

    length = (iter->second.endSeqNum - fromSeqNum);
    sacked = iter->second.sacked;
    rexmitted = iter->second.rexmitted;

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    checkSackBlockTime += std::chrono::duration<double>(elapsed).count();

    iter++;
}

void TcpSackRexmitQueue::checkSackBlock(uint32_t fromSeqNum, uint32_t& length, bool& sacked, bool& rexmitted) //const
{
    auto start = std::chrono::high_resolution_clock::now();
    //ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));

    //auto iter = rexmitMap.upper_bound(fromSeqNum);
    auto iter = rexmitMap.at(fromSeqNum+1448);
    length = (iter.endSeqNum - fromSeqNum);
    sacked = iter.sacked;
    rexmitted = iter.rexmitted;

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    checkSackBlockTime += std::chrono::duration<double>(elapsed).count();
}

std::map<uint32_t, TcpSackRexmitQueue::Region>::iterator TcpSackRexmitQueue::searchSackBlock(uint32_t fromSeqNum) //const
{
    auto start = std::chrono::high_resolution_clock::now();
    //ASSERT(seqLE(begin, fromSeqNum) && seqLess(fromSeqNum, end));
    return rexmitMap.upper_bound(fromSeqNum);
}

uint32_t TcpSackRexmitQueue::getInFlight()
{
    if((m_sentSize - (m_sackedOut + m_lostOut) + m_retrans) > 6000000){
        std::cout << "\n BYTES IN FLIGHT: " << (m_sentSize - (m_sackedOut + m_lostOut) + m_retrans) << endl;
        std::cout << "\n SENT SIZE: " << m_sentSize  << endl;
        std::cout << "\n SACKED OUT SIZE: " << m_sackedOut  << endl;
        std::cout << "\n LOST SIZE: " << m_lostOut  << endl;
        std::cout << "\n RETRANS SIZE: " << m_retrans  << endl;
    }
    return m_sentSize - (m_sackedOut + m_lostOut) + m_retrans;
    //return m_sentSizel
}

void TcpSackRexmitQueue::skbSent(uint32_t seqNum, simtime_t m_firstSentTime, simtime_t m_lastSentTime, simtime_t m_deliveredTime, bool m_isAppLimited, uint32_t m_delivered, uint32_t m_appLimited)
{
    inet::tcp::TcpSackRexmitQueue::Region& region = rexmitMap.at(seqNum);
    region.m_firstSentTime = m_firstSentTime;
    region.m_lastSentTime = m_lastSentTime;
    region.m_deliveredTime = m_deliveredTime;
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
    return !(rexmitMap.find(seqNum) == rexmitMap.end());
}

void TcpSackRexmitQueue::consistencyCheck() const
{
    uint32_t sacked = 0;
    uint32_t lost = 0;
    uint32_t retrans = 0;

    for (auto it = rexmitMap.begin(); it != rexmitMap.end(); ++it)
    {
        if (it->second.sacked)
        {
            sacked += rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }
        if (it->second.lost)
        {
            lost += rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
        }
        if (it->second.rexmitted)
        {
            retrans += rexmitMap.begin()->second.endSeqNum - rexmitMap.begin()->second.beginSeqNum;
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
    return rexmitMap.begin()->second.endSeqNum;
}



} // namespace tcp

} // namespace inet
