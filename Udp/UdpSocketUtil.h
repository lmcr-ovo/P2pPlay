#ifndef P2PPLAY_UDPSOCKETUTIL_H
#define P2PPLAY_UDPSOCKETUTIL_H

#include <QUdpSocket>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <mswsock.h>   // SIO_UDP_CONNRESET（MinGW 里它在 mswsock.h，不在 mstcpip.h）
#endif

// Windows 下 UDP socket 收到 ICMP Port Unreachable 会触发 WSAECONNRESET，
// 并可能干扰后续正常 UDP 包的接收。这里禁用它（必须在 bind 之后调用）。
inline void disableUdpConnReset(QUdpSocket* sock) {
#ifdef Q_OS_WIN
    if (sock == nullptr || sock->socketDescriptor() == -1) {
        return;
    }

    DWORD bytesReturned = 0;
    BOOL newBehavior = FALSE;

    WSAIoctl(
            static_cast<SOCKET>(sock->socketDescriptor()),
            SIO_UDP_CONNRESET,
            &newBehavior,
            sizeof(newBehavior),
            nullptr,
            0,
            &bytesReturned,
            nullptr,
            nullptr
    );
#else
    Q_UNUSED(sock);
#endif
}

#endif // P2PPLAY_UDPSOCKETUTIL_H