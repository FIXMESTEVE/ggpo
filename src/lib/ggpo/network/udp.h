/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_H
#define _UDP_H

#include "poll.h"
#include "udp_msg.h"
#include "ggponet.h"
#include "ring_buffer.h"

#include "eos_p2p.h"

#define MAX_UDP_ENDPOINTS     16

static const int MAX_UDP_PACKET_SIZE = 4096;

//EOS_HP2P _hP2P;

class Udp : public IPollSink
{
public:
   struct Stats {
      int      bytes_sent;
      int      packets_sent;
      float    kbps_sent;
   };

   struct Callbacks {
      virtual ~Callbacks() { }
      virtual void OnMsg(EOS_ProductUserId &from, UdpMsg *msg, int len) = 0;
   };

   EOS_ProductUserId _localProductUserId; //TODO: init this somewhere??

protected:
   void Log(const char *fmt, ...);

public:
   Udp();

   void Init(uint16 port, Poll *p, Callbacks *callbacks, EOS_HPlatform hPlatform, EOS_ProductUserId localProductUserId);
   
   void SendTo(char *buffer, int len, int flags, EOS_ProductUserId to);

   virtual bool OnLoopPoll(void *cookie);

public:
   ~Udp(void);

protected:
   // Network transmission information
   //SOCKET         _socket;
    EOS_P2P_SocketId _socket;

   // state management
   Callbacks      *_callbacks;
   Poll           *_poll;
};

#endif
