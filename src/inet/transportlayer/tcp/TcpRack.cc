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

#include "TcpRack.h"

namespace inet {
namespace tcp {

TcpRack::TcpRack() {
    // TODO Auto-generated constructor stub

}

TcpRack::~TcpRack() {
    // TODO Auto-generated destructor stub
}

bool TcpRack::sentAfter(simtime_t t1, simtime_t t2, uint32_t seq1, uint32_t seq2)
{
    return (t1 > t2 || (t1 == t2 && seq1 > seq2));
}

void TcpRack::updateStats(uint32_t tser, bool retrans, simtime_t xmitTs, uint32_t endSeq, uint32_t sndNxt, simtime_t lastRtt)
{
  // Calculate RTT
  simtime_t rtt = lastRtt;

  double tserTime = (tser/1000);

  // Check if the ACK was for a retransmitted packet. Also if it was a spurious retransmission
  if (retrans)
    {
      if (tserTime < xmitTs.dbl())
        {
          return;
        }

      if (rtt < m_minRtt)
        {
            return;
        }
    }

  if (rtt > 0)
    {
      m_rackRtt = rtt;
      m_rttSeq = sndNxt;
    }

  if (m_minRtt == 0)
    {
      m_minRtt = m_rackRtt;
    }
  else
    {
      m_minRtt = std::min(m_minRtt, m_rackRtt);
    }

  if (m_srtt == 0)
    {
      m_srtt = m_rackRtt.dbl();
    }
  else
    {
      m_srtt = (1 - m_alpha) * m_srtt + m_alpha * m_rackRtt.dbl();
    }

  if (sentAfter(xmitTs, m_rackXmitTs, endSeq, m_rackEndSeq))
    {
      m_rackXmitTs = xmitTs;
      m_rackEndSeq = endSeq;
    }
}

void TcpRack::updateReoWnd(bool reordSeen, bool dsackSeen, uint32_t sndNxt, uint32_t sndUna, uint32_t sacked, uint32_t dupAckThresh, bool exiting, bool lossRecovery)
{
  if (dsackSeen)
    {
      m_dsack = true;
    }

  // React to DSACK once per round trip
  if (sndUna < m_rttSeq)
    {
      m_dsack = false;
    }

  if (m_dsack)
    {
      m_reoWndIncr++;
      m_dsack = false;
      m_rttSeq = sndNxt;
      m_reoWndPersist = 16;      // Keep window for 16 recoveries
    }
  else if (exiting)              // Exiting Loss Recovery
    {
      m_reoWndPersist--;
      if (m_reoWndPersist <= 0)
        {
          m_reoWndIncr = 1;
        }
    }

  if (!reordSeen)
    {
      bool congState = false; //TODO FIX CONG STATE, SHOULD CHECK LOSS RECOVERY
      if (lossRecovery || (sacked >= dupAckThresh*1448))
        {
          m_reoWnd = 0;
          return;
        }
    }
  m_reoWnd = (m_minRtt.dbl() / 4) * m_reoWndIncr;
  m_reoWnd = std::min (m_reoWnd, m_srtt);
}

} /* namespace tcp */
} /* namespace inet */
