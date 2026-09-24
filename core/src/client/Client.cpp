// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#include "Client.h"

#include "Logger.h"
#include "StringUtils.h"

#include <thread>
#include <cstring>
#include <exception>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#endif

namespace MagicPodsCore {
    Client::Client(const std::string& address, unsigned short port, ClientConnectionType connectionType)
        : _address{address}, _port{port}, _connectionType{connectionType} {}

    Client::Client(const std::string& address, const std::string& serviceUuid, ClientConnectionType connectionType)
        : _address{address}, _serviceUuid{serviceUuid}, _connectionType{connectionType} {}

    bool Client::Start(const std::function<void(Client&)>& justAfterStartLogic) {
        std::lock_guard lockGuard{_startStopMutex};

        if (_isStarted)
            return true;
        if (_connectionType == ClientConnectionType::L2CAP && !SupportsL2CAP()) {
            Logger::Info("%s L2CAP needs a kernel driver on this platform, settings over AAP stay unavailable", _address.c_str());
            return false;
        }
        _isStarted = true;

        Logger::Info("%s Start Bluetooth client", _address.c_str());

        /* connect to server */
        if(!ConnectToSocket(CONNECTION_TO_SOCKET_ATTEMPTS_NUMBER)) {
            _isStarted = false;
            Logger::Error("%s Connect to socket is failed.",_address.c_str());
            return false;
        }
        Logger::Info("%s connected", _address.c_str());

        JoinThreads(); // a std::thread still holding a finished session would terminate() on reassignment
        _outcomeMessagesQueue.Reopen();

        _writingThread = std::thread([this]() {
            while (_isStarted) {
                const auto data = _outcomeMessagesQueue.Take();

                if (!data.has_value())
                    break;

                SocketSend(data.value().data(), data.value().size());
                Logger::Debug("s:%s",StringUtils::BytesToHexString(data.value().data(), data.value().size()).c_str());
                //std::this_thread::sleep_for(std::chrono::milliseconds{500}); //Return if the user's feedback is bad.
            }

            Logger::Debug("%s Writing thread stopped", _address.c_str());
        });

        _readingThread = std::thread([this]() {
            unsigned char buffer[1024];
            std::vector<unsigned char> vectorBuffer(1024); // optimize
            while(_isStarted) {
                memset(buffer, 0, sizeof(buffer));
                long receivedBytesLength = SocketReceive(buffer, sizeof(buffer));
                if (receivedBytesLength > 0) {
                    Logger::Trace("r:%s", StringUtils::BytesToHexString(buffer, receivedBytesLength).c_str());
                    vectorBuffer.assign(buffer, buffer + receivedBytesLength);

                    _onReceivedDataEvent.FireEvent(vectorBuffer);
                }
                else {
                    Logger::Debug("%s stop listening", _address.c_str());
                    break;
                }
            }

            Logger::Debug("%s Reading thread stopped", _address.c_str());
        });

        if (justAfterStartLogic)
            justAfterStartLogic(*this);
        return true;
    }

    void Client::Stop() {
        std::lock_guard lockGuard{_startStopMutex};

        if (!_isStarted)
            return;
        _isStarted = false;

        SocketShutdown();
        _outcomeMessagesQueue.Close();
        JoinThreads();
        SocketClose(); // only now: the number must not be reused while a thread still uses it

        Logger::Info("Stop Bluetooth client, server addr %s", _address.c_str());
    }

    void Client::JoinThreads() {
        for (auto* thread : {&_writingThread, &_readingThread}) {
            if (!thread->joinable())
                continue;
            // a received packet may lead to Stop() on the reading thread itself
            if (thread->get_id() == std::this_thread::get_id())
                thread->detach();
            else
                thread->join();
        }
    }

    void Client::SendData(const std::vector<unsigned char>& data) {
        _outcomeMessagesQueue.Put(data);
    }

    bool Client::ConnectToSocket(int attemptsNumber) {
        bool isConnected{false};

        while (true) {
            --attemptsNumber;
            Logger::Info("%s Attempt to connect. Left %d", _address.c_str(), attemptsNumber);
            switch (_connectionType)
            {
            case ClientConnectionType::L2CAP:
                isConnected = ConnectToSocketL2CAP();
                break;
            case ClientConnectionType::RFCOMM:
                isConnected = ConnectToSocketRFCOMM();
                break;
            }
            if (attemptsNumber <= 0 || isConnected || !_isStarted) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1)); // TODO: вероятно delay можно выставить для connect()
        }
        return isConnected;
    }

#ifndef _WIN32
    bool Client::SupportsL2CAP() {
        return true;
    }

    long Client::SocketSend(const unsigned char* data, size_t length) {
        return send(_socket, data, length, 0);
    }

    long Client::SocketReceive(unsigned char* buffer, size_t length) {
        return recv(_socket, buffer, length, 0);
    }

    void Client::SocketShutdown() {
        if (_socket >= 0)
            shutdown(_socket, SHUT_RDWR);
    }

    void Client::SocketClose() {
        if (_socket >= 0)
            close(_socket);
        _socket = -1;
    }

    bool Client::ConnectToSocketL2CAP() {
        struct sockaddr_l2 addr = { 0 };

        /* allocate a socket */
        _socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
        if (_socket < 0)
            return false;

        /* set the outgoing connection parameters, server's address and port number */
        addr.l2_family = AF_BLUETOOTH;								/* Addressing family, always AF_BLUETOOTH */
        addr.l2_psm = htobs(0x1001);					/* server's port number */
        str2ba(_address.c_str(), &addr.l2_bdaddr);		/* server's Bluetooth Address */

        /* connect to server */
        if(connect(_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            SocketClose(); // every failed attempt would otherwise leak a descriptor
            return false;
        }

        return true;
    }

    bool Client::ConnectToSocketRFCOMM() {
        Logger::Debug("%s is trying to connect to %s", _address.c_str(), _serviceUuid.c_str());

        if (_serviceUuid.empty()){
            Logger::Error("Failed to connect. UUID is empty.");
            return false;
        }

        struct sockaddr_rc addr = { 0 };

        /* retreiving the port */
        uint8_t uuid_bytes[16] = {0};
        StringUtils::UuidStringToBytes(_serviceUuid.c_str(), uuid_bytes);
        const auto optionalPort = RetrieveServicePortRFCOMM(uuid_bytes, _address.c_str());
        if (!optionalPort.has_value()) {
            return false;
        }
        _port = optionalPort.value();

        /* allocate a socket */
        _socket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        if (_socket < 0)
            return false;

        // set the connection parameters (who to connect to)
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = _port;
        str2ba(_address.c_str(), &addr.rc_bdaddr);

        /* connect to server */
        if(connect(_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            SocketClose();
            return false;
        }

        return true;
    }

    std::optional<uint8_t> Client::RetrieveServicePortRFCOMM(uint8_t* uuid, const char* deviceAddress)
    {
        // connect to an SDP server
        uint8_t address[6];
        str2ba(deviceAddress, (bdaddr_t*)&address);

        bdaddr_t tmp = { 0, 0, 0, 0, 0, 0 };

        sdp_session_t* session = sdp_connect(&tmp, (bdaddr_t*)&address, SDP_RETRY_IF_BUSY);
        if (!session) {
            Logger::Error("%s SDP can't connect to sdp server!", deviceAddress);
            return std::nullopt;
        }

        uuid_t uuid128;
        sdp_uuid128_create(&uuid128, uuid);

        // create query lists
        int range = 0x0000ffff;
        sdp_list_t* responseList;
        sdp_list_t* searchList = sdp_list_append(nullptr, &uuid128);
        sdp_list_t* attrIdList = sdp_list_append(nullptr, &range);

        // search for records
        int success = sdp_service_search_attr_req(
            session, searchList, SDP_ATTR_REQ_RANGE, attrIdList, &responseList);
        sdp_list_free(searchList, nullptr);
        sdp_list_free(attrIdList, nullptr);
        sdp_close(session); // runs on every connection attempt, so nothing may leak past here
        if (success) {
            Logger::Error("%s SDP search failed!", deviceAddress);
            return std::nullopt;
        }

        // check responses
        success = sdp_list_len(responseList);
        if (success <= 0) {
            Logger::Error("%s SDP no responses!", deviceAddress);
            sdp_list_free(responseList, (sdp_free_func_t)sdp_record_free);
            return std::nullopt;
        }

        // process responses
        std::optional<uint8_t> channel{std::nullopt};
        sdp_list_t* responses = responseList;
        while (responses) {
            auto* record = (sdp_record_t*)responses->data;

            sdp_list_t* protoList;
            success = sdp_get_access_protos(record, &protoList);
            if (success) {
                Logger::Error("%s SDP can't access protocols!", deviceAddress);
                responses = responses->next;
                continue;
            }

            sdp_list_t* protocol = protoList;
            while (protocol) {
                sdp_list_t* pds;
                int protocolCount = 0;
                pds = (sdp_list_t*)protocol->data;

                while (pds) { // loop thru all pds
                    sdp_data_t* d;
                    int dtd;
                    d = (sdp_data_t*)pds->data;
                    while (d) {
                        dtd = d->dtd;
                        switch (dtd) {
                        case SDP_UUID16:
                        case SDP_UUID32:
                        case SDP_UUID128:
                            protocolCount = sdp_uuid_to_proto(&d->val.uuid);
                            break;
                        case SDP_UINT8:
                            if (protocolCount == RFCOMM_UUID) {
                                channel = d->val.uint8; // save channel id
                            }
                            break;
                        default:
                            break;
                        }
                        d = d->next; // to next data unit
                    }
                    pds = pds->next; // to next pds
                }
                sdp_list_free((sdp_list_t*)protocol->data, nullptr);

                protocol = protocol->next; // to next protocol
            }
            sdp_list_free(protoList, nullptr);

            responses = responses->next; // to next response
        }
        sdp_list_free(responseList, (sdp_free_func_t)sdp_record_free);

        return channel;
    }
#endif

    std::unique_ptr<Client> Client::CreateL2CAP(const std::string& address, unsigned short port) {
        return std::unique_ptr<Client>(new Client(address, port, ClientConnectionType::L2CAP));
    }

    std::unique_ptr<Client> Client::CreateRFCOMM(const std::string& address, const std::string& serviceUuid) {
        return std::unique_ptr<Client>(new Client(address, serviceUuid, ClientConnectionType::RFCOMM));
    }

    Client::~Client()
    {
        Stop();
        Logger::Debug("Client::~Client()");
    }
}