// MyPods
// License: GPL-3.0

// Windows side of Client: RFCOMM through Winsock (AF_BTH). Windows resolves the RFCOMM channel from the
// service UUID itself (SDP), so there is no counterpart to RetrieveServicePortRFCOMM.

#include "Client.h"
#include "Logger.h"

#include <cstdio>
#include <winsock2.h>
#include <ws2bth.h>

namespace MagicPodsCore {

    namespace {
        void EnsureWinsock() {
            static const bool started = [] {
                WSADATA data;
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
            if (!started)
                Logger::Error("Winsock could not be started");
        }

        BTH_ADDR ParseAddress(const std::string& address) {
            unsigned int b[6]{};
            std::sscanf(address.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
            BTH_ADDR result = 0;
            for (unsigned int byte : b)
                result = (result << 8) | byte;
            return result;
        }

        bool ParseGuid(const std::string& uuid, GUID& guid) {
            unsigned long data1;
            unsigned int data2, data3, d[8];
            if (std::sscanf(uuid.c_str(), "%8lx-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &data1, &data2, &data3,
                            &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]) != 11)
                return false;
            guid.Data1 = data1;
            guid.Data2 = static_cast<unsigned short>(data2);
            guid.Data3 = static_cast<unsigned short>(data3);
            for (int i = 0; i < 8; i++)
                guid.Data4[i] = static_cast<unsigned char>(d[i]);
            return true;
        }
    }

    bool Client::SupportsL2CAP() {
        return false;
    }

    bool Client::ConnectToSocketL2CAP() {
        Logger::Error("%s L2CAP is not available without a kernel driver on Windows", _address.c_str());
        return false;
    }

    bool Client::ConnectToSocketRFCOMM() {
        Logger::Debug("%s is trying to connect to %s", _address.c_str(), _serviceUuid.c_str());

        SOCKADDR_BTH addr{};
        if (!ParseGuid(_serviceUuid, addr.serviceClassId)) {
            Logger::Error("Failed to connect. Invalid UUID: %s", _serviceUuid.c_str());
            return false;
        }
        addr.addressFamily = AF_BTH;
        addr.btAddr = ParseAddress(_address);
        addr.port = 0; // 0: channel looked up by serviceClassId

        EnsureWinsock();
        SOCKET s = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        if (s == INVALID_SOCKET) {
            Logger::Error("%s RFCOMM socket failed: %d", _address.c_str(), WSAGetLastError());
            return false;
        }
        _socket = static_cast<std::intptr_t>(s);
        if (connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
            Logger::Error("%s RFCOMM connect failed: %d", _address.c_str(), WSAGetLastError());
            closesocket(s);
            _socket = -1;
            return false;
        }
        return true;
    }

    long Client::SocketSend(const unsigned char* data, size_t length) {
        return send(static_cast<SOCKET>(_socket), reinterpret_cast<const char*>(data), static_cast<int>(length), 0);
    }

    long Client::SocketReceive(unsigned char* buffer, size_t length) {
        return recv(static_cast<SOCKET>(_socket), reinterpret_cast<char*>(buffer), static_cast<int>(length), 0);
    }

    void Client::SocketShutdown() {
        if (_socket != -1)
            shutdown(static_cast<SOCKET>(_socket), SD_BOTH);
    }

    void Client::SocketClose() {
        if (_socket != -1)
            closesocket(static_cast<SOCKET>(_socket));
        _socket = -1;
    }
}
