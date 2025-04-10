//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef TRANSPORTLAYER_TCP_TCPRACK_H_
#define TRANSPORTLAYER_TCP_TCPRACK_H_

#include <inet/transportlayer/tcp/TcpConnection.h>

namespace inet {
namespace tcp {

class TcpRack
{
public:
    TcpRack();
    virtual ~TcpRack();

    /**
     *  Checks if packet1 sent after packet2
     */
    bool sentAfter(simtime_t t1, simtime_t t2, uint32_t seq1, uint32_t seq2);

    /**
     * Updates reo_wnd
     */
    void updateReoWnd(bool reordSeen, bool dsackSeen, uint32_t sndNxt, uint32_t sndUna, uint32_t sacked, uint32_t dupAckThresh, bool exiting, bool lossRecovery);

    /**
     *  Updates RACK parameters
     */
    void updateStats(uint32_t tser, bool retrans, simtime_t xmitTs, uint32_t endSseq, uint32_t sndNxt, simtime_t lastRtt);

    double getReoWnd() { return m_reoWnd;}

    simtime_t getXmitTs() { return m_rackXmitTs;}

    double getEndSeq() { return m_rackEndSeq;}

    simtime_t getRtt(){ return m_rackRtt;}
protected:
    simtime_t m_rackXmitTs = 0; //!< Latest transmission timestamp of Rack.packet
    uint32_t m_rackEndSeq = 0;//!< Ending sequence number of Rack.packet
    simtime_t m_rackRtt = 0; //!< RTT of the most recently transmitted packet that has been acknowledged
    double m_reoWnd = 0; //!< Re-ordering Window
    simtime_t m_minRtt = 0; //!< Minimum RTT
    uint32_t m_rttSeq = 0; //!< SND.NXT when RACK.rtt is updated
    bool m_dsack = false; //!< If a DSACK option has been received since last RACK.reo_wnd change
    uint32_t m_reoWndIncr = 1; //!< Multiplier applied to adjust RACK.reo_wnd
    uint32_t m_reoWndPersist = 16; //!< Number of loss recoveries before resetting RACK.reo_wnd
    double m_srtt = 0; //!< Smoothened RTT (SRTT) as specified in [RFC6298]
    double m_alpha = 0.125; //!< EWMA constant for calculation of SRTT (alpha = 1/8)
};

} /* namespace tcp */
} /* namespace inet */

#endif /* INET_TRANSPORTLAYER_TCP_TCPRACK_H_ */
