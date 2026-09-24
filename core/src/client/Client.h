// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "IRequest.h"
#include "ClientConnectionType.h"
#include "Event.h"
#include "BlockingQueue.h"

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <thread>
#include <array>
#include <memory>
#include <optional>
#include <mutex>
#include <functional>

namespace MagicPodsCore {

    class Client {
    private:
        static const int CONNECTION_TO_SOCKET_ATTEMPTS_NUMBER = 1;

        std::string _address{};
        unsigned short _port{};
        std::string _serviceUuid{};

        ClientConnectionType _connectionType{};

        std::intptr_t _socket{-1}; // fd on Linux, SOCKET on Windows
        bool _isStarted{false};

        std::mutex _startStopMutex{};

        BlockingQueue<std::vector<unsigned char>> _outcomeMessagesQueue{};

        Event<std::vector<unsigned char>> _onReceivedDataEvent{};

    public:
        // AAP needs an L2CAP channel. Linux opens one from user space; Windows only allows it from a
        // kernel-mode profile driver: true there once the MyPods AAP driver (driver/windows) is installed.
        static bool SupportsL2CAP();

        void Start(const std::function<void(Client&)>& justAfterStartLogic = {});
        void Stop();

        bool IsStarted() const {
            return _isStarted;
        }

        Event<std::vector<unsigned char>>& GetOnReceivedDataEvent() {
            return _onReceivedDataEvent;
        }

        void SendData(const std::vector<unsigned char>& data);

    private:
        bool ConnectToSocketL2CAP();
        bool ConnectToSocketRFCOMM();
        bool ConnectToSocket(int attemptsNumber);
        // Platform socket calls, byte count or <= 0 on error/close like send()/recv()
        long SocketSend(const unsigned char* data, size_t length);
        long SocketReceive(unsigned char* buffer, size_t length);
        void SocketClose();
#ifndef _WIN32
        static std::optional<uint8_t> RetrieveServicePortRFCOMM(uint8_t* uuid, const char* deviceAddress);
#endif

    private:
        explicit Client(const std::string& address, unsigned short port, ClientConnectionType connectionType);
        explicit Client(const std::string& address, const std::string& serviceUuid, ClientConnectionType connectionType);

    public:
        static std::unique_ptr<Client> CreateL2CAP(const std::string& address, unsigned short port);
        static std::unique_ptr<Client> CreateRFCOMM(const std::string& address, const std::string& serviceUuid);
        ~Client();
    };
}
