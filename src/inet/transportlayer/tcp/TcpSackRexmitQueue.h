//
// Copyright (C) 2009-2010 Thomas Reschka
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TCPSACKREXMITQUEUE_H
#define __INET_TCPSACKREXMITQUEUE_H

#include "inet/transportlayer/tcp/TcpConnection.h"
#include "inet/transportlayer/tcp_common/TcpHeader.h"
#include <tuple>
namespace inet {
namespace tcp {

/**
 * Retransmission data for SACK.
 */
class INET_API TcpSackRexmitQueue
{
  public:
    TcpConnection *conn; // the connection that owns this queue

    struct Region {
        uint32_t beginSeqNum;
        uint32_t endSeqNum;
        bool sacked; // indicates whether region has already been sacked by data receiver
        bool rexmitted; // indicates whether region has already been retransmitted by data sender
        int numOfDiscontiguousSacks;
        bool lost;

        uint32_t m_delivered;
        simtime_t m_firstSentTime;
        simtime_t m_lastSentTime;
        simtime_t m_deliveredTime;
        uint32_t m_bytes;
        bool m_isAppLimited;
    };

    typedef std::list<Region> RexmitQueue;
    RexmitQueue rexmitQueue; // rexmitQueue is ordered by seqnum, and doesn't have overlapped Regions
    std::map<uint32_t, Region> rexmitMap;
    std::map<uint32_t, Region> backupRexmitMap;
    uint32_t begin; // 1st sequence number stored
    uint32_t end; // last sequence number stored + 1

    uint32_t m_sentSize;
    uint32_t m_sackedOut;
    uint32_t m_lostOut;
    uint32_t m_retrans;

  public:
    /**
     * Ctor
     */
    TcpSackRexmitQueue();

    /**
     * Virtual dtor.
     */
    virtual ~TcpSackRexmitQueue();

    /**
     * Set the connection that owns this queue.
     */
    virtual void setConnection(TcpConnection *_conn) { conn = _conn; }

    /**
     * Initialize the object. The startSeq parameter tells what sequence number the first
     * byte of app data should get. This is usually ISS + 1 because SYN consumes
     * one byte in the sequence number space.
     *
     * init() may be called more than once; every call flushes the existing contents
     * of the queue.
     */
    virtual void init(uint32_t seqNum);

    /**
     * Returns a string for debug purposes.
     */
    virtual std::string str() const;

    /**
     * Prints the current rexmitQueue status for debug purposes.
     */
    virtual std::string detailedInfo() const;

    /**
     * Returns the sequence number of the first byte stored in the buffer.
     */
    virtual uint32_t getBufferStartSeq() const { return rexmitMap.begin()->second.beginSeqNum; }

    /**
     * Returns the sequence number of the last byte stored in the buffer plus one.
     * (The first byte of the next send operation would get this sequence number.)
     */
    virtual uint32_t getBufferEndSeq() const { return (--rexmitMap.end())->second.endSeqNum; }

    /**
     * Tells the queue that bytes up to (but NOT including) seqNum have been
     * transmitted and ACKed, so they can be removed from the queue.
     */
    virtual void discardUpTo(uint32_t seqNum);

    /**
     * Inserts sent data to the rexmit queue.
     */
    virtual void enqueueSentData(uint32_t fromSeqNum, uint32_t toSeqNum);

    /**
     * Called when data sender received selective acknowledgments.
     * Tells the queue which bytes have been transmitted and SACKed,
     * so they can be skipped if retransmitting segments as long as
     * REXMIT timer did not expired.
     */
    virtual void setSackedBit(uint32_t fromSeqNum, uint32_t toSeqNum);

    virtual std::list<uint32_t> setSackedBitList(uint32_t fromSeqNum, uint32_t toSeqNum);


    /**
     * Returns SackedBit value of seqNum.
     */
    virtual bool getSackedBit(uint32_t seqNum); //const;

    /**
     * Returns the number of blocks currently buffered in queue.
     */
    virtual uint32_t getQueueLength() const { return rexmitQueue.size(); }

    /**
     * Returns the highest sequence number sacked by data receiver.
     */
    virtual uint32_t getHighestSackedSeqNum(); //const;

    /**
     * Returns the highest sequence number rexmitted by data sender.
     */
    virtual uint32_t getHighestRexmittedSeqNum(); //const;

    /**
     * Checks rexmit queue for sacked of rexmitted segments and returns a certain offset
     * (contiguous sacked or rexmitted region) to forward snd->nxt.
     * It is called before retransmitting data.
     */
    virtual uint32_t checkRexmitQueueForSackedOrRexmittedSegments(uint32_t fromSeq); //const;

    /**
     * Called when REXMIT timer expired.
     * Resets sacked bit of all segments in rexmit queue.
     */
    virtual void resetSackedBit();

    /**
     * Called when REXMIT timer expired.
     * Resets rexmitted bit of all segments in rexmit queue.
     */
    virtual void resetRexmittedBit();

    /**
     * Returns total amount of sacked bytes. Corresponds to update() function from RFC 3517.
     */
    virtual uint32_t getTotalAmountOfSackedBytes(); //const;

    /**
     * Returns amount of sacked bytes above seqNum.
     */
    virtual uint32_t getAmountOfSackedBytes(uint32_t seqNum); //const;

    virtual bool checkIsLost(uint32_t seqNo, uint32_t highestSackedSeqNum);

    virtual bool checkHeadIsLost();

    virtual std::tuple<bool, bool> getLostAndRetransmitted(uint32_t seqNo);

    virtual void updateLost(uint32_t highestSackedSeqNum);

    virtual void consistencyCheck() const;
    /**
     * Returns the number of discontiguous sacked regions (SACKed sequences) above seqNum.
     */
    virtual uint32_t getNumOfDiscontiguousSacks(uint32_t seqNum); //const;

    /*
     * Returns nothing but checks length, sacked bit and rexmitted bit of a given
     * SACK block starting at seqNum.
     */
    virtual void checkSackBlock(uint32_t seqNum, uint32_t& length, bool& sacked, bool& rexmitted);// const;

    virtual void checkSackBlockLost(uint32_t seqNum, uint32_t& length, bool& sacked, bool& rexmitted, bool&lost);// const;

    virtual std::map<uint32_t, Region>::iterator searchSackBlock(uint32_t seqNum);// const;

    virtual uint32_t getInFlight();

    virtual uint32_t getLost();

    virtual void markHeadAsLost();

    virtual void setAllLost();

    virtual void skbSent(uint32_t seqNum, simtime_t m_firstSentTime, simtime_t m_lastSentTime, simtime_t m_deliveredTime, bool m_IsAppLimited, uint32_t m_delivered, uint32_t m_appLimited);

    virtual Region& getRegion(uint32_t seqNum);

    virtual bool findRegion(uint32_t seqNum);

    virtual uint32_t getTailSequence();

  protected:
    /*
     * Returns if TcpSackRexmitQueue is valid or not.
     */
    bool checkQueue() const;

};

} // namespace tcp
} // namespace inet

#endif
