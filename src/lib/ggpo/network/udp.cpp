/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "udp.h"
#include <eos_sdk.h>

#define EOS

EOS_HP2P g_hP2P;

SOCKET
CreateSocket(uint16 bind_port, int retries)
{
   //todo: create the socket id and store it
   //preemptively accept connections

   SOCKET s;
   sockaddr_in sin;
   uint16 port;
   int optval = 1;

   s = socket(AF_INET, SOCK_DGRAM, 0);
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof optval);
   setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (const char *)&optval, sizeof optval);

   // non-blocking...
   u_long iMode = 1;
   ioctlsocket(s, FIONBIO, &iMode);

   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   for (port = bind_port; port <= bind_port + retries; port++) {
      sin.sin_port = htons(port);
      if (bind(s, (sockaddr *)&sin, sizeof sin) != SOCKET_ERROR) {
         Log("Udp bound to port: %d.\n", port);
         return s;
      }
   }
   closesocket(s);
   return INVALID_SOCKET;
}

Udp::Udp() :
   _callbacks(NULL)
{
}

Udp::~Udp(void)
{
    EOS_P2P_CloseConnectionsOptions oCloseConnections = {};
    oCloseConnections.ApiVersion = EOS_P2P_CLOSECONNECTIONS_API_LATEST;
    oCloseConnections.LocalUserId = _localProductUserId;
    oCloseConnections.SocketId = &_socket;
    EOS_P2P_CloseConnections(g_hP2P, &oCloseConnections);
}

void
Udp::Init(uint16 port, Poll *poll, Callbacks *callbacks, EOS_HPlatform hPlatform, EOS_ProductUserId localProductUserId)
{
    _socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
    strncpy_s(_socket.SocketName, "CHAT", 5);

   _localProductUserId = localProductUserId;
   g_hP2P = EOS_Platform_GetP2PInterface(hPlatform);
   EOS_P2P_AddNotifyPeerConnectionRequestOptions oAddNotifyPeerConnectionRequest = {};
   oAddNotifyPeerConnectionRequest.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
   oAddNotifyPeerConnectionRequest.LocalUserId = _localProductUserId;
   oAddNotifyPeerConnectionRequest.SocketId = &_socket;
   EOS_P2P_AddNotifyPeerConnectionRequest(g_hP2P, &oAddNotifyPeerConnectionRequest, NULL, [](const EOS_P2P_OnIncomingConnectionRequestInfo* data) {
       EOS_P2P_AcceptConnectionOptions oAcceptConnection = {};
       oAcceptConnection.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
       oAcceptConnection.LocalUserId = data->LocalUserId;
       oAcceptConnection.RemoteUserId = data->RemoteUserId;
       oAcceptConnection.SocketId = data->SocketId;
       EOS_P2P_AcceptConnection(g_hP2P, &oAcceptConnection);
   });

   _callbacks = callbacks;

   _poll = poll;
   _poll->RegisterLoop(this);
}

void
Udp::SendTo(char *buffer, int len, int flags, EOS_ProductUserId to)
{
#if defined(EOS)
    EOS_P2P_SendPacketOptions oSendPacketOptions = {};
    oSendPacketOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
    oSendPacketOptions.bAllowDelayedDelivery = EOS_FALSE;
    oSendPacketOptions.Channel = 0;
    oSendPacketOptions.Data = buffer;
    oSendPacketOptions.DataLengthBytes = len;
    oSendPacketOptions.LocalUserId = _localProductUserId;
    oSendPacketOptions.Reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
    oSendPacketOptions.RemoteUserId = to;
    oSendPacketOptions.SocketId = &_socket;
    EOS_P2P_SendPacket(g_hP2P, &oSendPacketOptions);
#else
   struct sockaddr_in *to = (struct sockaddr_in *)dst;

   int res = sendto(_socket, buffer, len, flags, dst, destlen);
   if (res == SOCKET_ERROR) {
      DWORD err = WSAGetLastError();
      Log("unknown error in sendto (erro: %d  wsaerr: %d).\n", res, err);
      ASSERT(FALSE && "Unknown error in sendto");
   }
   char dst_ip[1024];
   Log("sent packet length %d to %s:%d (ret:%d).\n", len, inet_ntop(AF_INET, (void *)&to->sin_addr, dst_ip, ARRAY_SIZE(dst_ip)), ntohs(to->sin_port), res);
#endif
}

bool
Udp::OnLoopPoll(void *cookie)
{
   uint8          recv_buf[MAX_UDP_PACKET_SIZE];
   sockaddr_in    recv_addr;
   int            recv_addr_len;

   for (;;) {
      recv_addr_len = sizeof(recv_addr);
      //int len = recvfrom(_socket, (char *)recv_buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
      uint32_t len;
      uint8_t OutChannel;

      EOS_P2P_SocketId OutSocketId = {};
      EOS_ProductUserId OutPeerId = {};
      EOS_P2P_ReceivePacketOptions oReceivePacketOptions = {};
      oReceivePacketOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
      oReceivePacketOptions.LocalUserId = _localProductUserId;
      oReceivePacketOptions.MaxDataSizeBytes = MAX_UDP_PACKET_SIZE;
      oReceivePacketOptions.RequestedChannel = 0;

      EOS_P2P_ReceivePacket(g_hP2P, &oReceivePacketOptions, &OutPeerId, &OutSocketId, &OutChannel, recv_buf, &len);

      // TODO: handle len == 0... indicates a disconnect.

      if (len == -1) {
         int error = WSAGetLastError();
         if (error != WSAEWOULDBLOCK) {
            Log("recvfrom WSAGetLastError returned %d (%x).\n", error, error);
         }
         break;
      } else if (len > 0) {
         //char src_ip[1024];
         //Log("recvfrom returned (len:%d  from:%s:%d).\n", len, inet_ntop(AF_INET, (void*)&recv_addr.sin_addr, src_ip, ARRAY_SIZE(src_ip)), ntohs(recv_addr.sin_port) );
         UdpMsg *msg = (UdpMsg *)recv_buf;
         _callbacks->OnMsg(OutPeerId, msg, len);
      } 
   }
   return true;
}


void
Udp::Log(const char *fmt, ...)
{
   char buf[1024];
   size_t offset;
   va_list args;

   strcpy_s(buf, "udp | ");
   offset = strlen(buf);
   va_start(args, fmt);
   vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
   buf[ARRAY_SIZE(buf)-1] = '\0';
   ::Log(buf);
   va_end(args);
}
