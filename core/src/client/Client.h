// MagicPodsCore: https://github.com/steam3d/MagicPodsCore
// Copyright: 2020-2025 Aleksandr Maslov <https://magicpods.app> & Andrei Litvintsev <a.a.litvintsev@gmail.com>
// License: GPL-3.0

#pragma once

#include "IRequest.h"
#include "ClientConnectionType.h"
#include "Event.h"
#include "BlockingQueue.h"

#include <atomic>
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
        static const int CONNECTION_TO_SOCKET_ATTEMPTS_NUMBER = 3;

        std::string _address{};
        unsigned short _port{};
        std::string _serviceUuid{};

        ClientConnectionType _connectionType{};

        std::intptr_t _socket{-1}; // fd on Linux, SOCKET on Windows
        std::atomic<bool> _isStarted{false};
        std::atomic<bool> _sessionEnded{false}; // the other side closed the channel; Start() opens a new one

        std::mutex _startStopMutex{};
        // joined in Stop(): a thread outliving a session would read the next session's socket or a destroyed Client
        std::thread _writingThread{};
        std::thread _readingThread{};

        BlockingQueue<std::vector<unsigned char>> _outcomeMessagesQueue{};

        Event<std::vector<unsigned char>> _onReceivedDataEvent{};
        Event<bool> _onClosedEvent{};

    public:
        // AAP needs an L2CAP channel. Linux opens one from user space; Windows only allows it from a
        // kernel-mode profile driver, which MyPods doesn't ship, so AAP features stay off there.
        static bool SupportsL2CAP();

        // false when the channel can't be opened (headphones busy, out of range, held by another tool);
        // the caller stays alive and may try again on the next connection
        bool Start(const std::function<void(Client&)>& justAfterStartLogic = {});
        void Stop();

        bool IsStarted() const {
            return _isStarted;
        }

        Event<std::vector<unsigned char>>& GetOnReceivedDataEvent() {
            return _onReceivedDataEvent;
        }

        // The headphones closed the channel while the Bluetooth link may still be up. Fired on the
        // reading thread, so listeners must not call Start() or Stop() from it.
        Event<bool>& GetOnClosedEvent() {
            return _onClosedEvent;
        }

        void SendData(const std::vector<unsigned char>& data);

    private:
        bool ConnectToSocketL2CAP();
        bool ConnectToSocketRFCOMM();
        bool ConnectToSocket(int attemptsNumber);
        // Platform socket calls, byte count or <= 0 on error/close like send()/recv()
        long SocketSend(const unsigned char* data, size_t length);
        long SocketReceive(unsigned char* buffer, size_t length);
        // wakes a recv() blocked in the reading thread; close() alone doesn't on Linux
        void SocketShutdown();
        void SocketClose();
        void JoinThreads();
        void StopLocked();
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
