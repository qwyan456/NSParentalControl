#include <limits>
#include "Server.hpp"
#include "../logger.h"

// This was heavily inspired by sys-clk's ipc server, a big thanks to those
// who wrote the original C version:
// --------------------------------------------------------------------------
// "THE BEER-WARE LICENSE" (Revision 42):
// <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
// wrote this file. As long as you retain this notice you can do whatever you
// want with this stuff. If you meet any of us some day, and you think this
// stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
// --------------------------------------------------------------------------
using namespace alefbet::pctrl::logger;
/*template <class T>
struct Mallocator {
    typedef T value_type;

    Mallocator() = default;

    template <class U>
    constexpr Mallocator(const Mallocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            logToFile("erreur alloc 1\n");
            return nullptr;
        }

        if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
            report(p, n);
            return p;
        }

        logToFile("erreur alloc 2\n");
        return nullptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        report(p, n, 0);
        std::free(p);
    }

   private:
    void report(T* p, std::size_t n, bool alloc = true) const {
        logToFile("%s bytes at %p\n", alloc ? "Alloc: " : "Dealloc: ", sizeof(T) * n, (void*)p);
    }
};*/

namespace Ipc {    
    constexpr uint64_t waitTimeout = UINT64_MAX;                // Wait timeout when processing

    Server::Server(const std::string & name) {
        // Set status variables
        this->error_ = false;
        this->handler = nullptr;
        //this->maxHandles = maxClients + 1;
        logToFile("1\n");
        //this->handles.reserve(this->maxHandles);
        logToFile("2\n");

        // Exit if invalid session count given
        /*if (maxClients < 1 || maxClients > MAX_WAIT_OBJECTS - 1) {
            logToFile("[IPC] Invalid number of sessions requested\n");
            this->error_ = true;
            return;
        }*/

        // Create server        
        Handle serverHandle;
        logToFile("3\n");
        this->serverName = smEncodeName(name.c_str());
        logToFile("4\n");
        ::Result rc = smRegisterService(&serverHandle, this->serverName, false, 1);
        logToFile("[IPC] IPC Server registered\n");
        if (R_FAILED(rc)) {
            logToFile("[IPC] Couldn't create server: %i\n", rc);
            return;
        }

        logToFile("[IPC] Server started\n");
        //this->handles.push_back(serverHandle);
        handles[0] = serverHandle;
        logToFile("5\n");
    }

    bool Server::processSession(const int32_t index) {
        int tmp;

        // Wait for request
        ::Result rc = svcReplyAndReceive(&tmp, &handles[index], 1, 0, UINT64_MAX);
        if (R_FAILED(rc)) {
            logToFile("[IPC] Couldn't receive request (closing handle): %i\n", rc);
            svcCloseHandle(this->handles[index]);
            handles[index] = 0;
            return true;        // Return true as closing a session is valid behaviour
        }

        // Create object from received data
        logToFile("c1\n");
        
        std::vector<uint8_t> vec;
        vec.reserve(10);
        logToFile("@vec=%p\n", (void*)vec.data());
        std::array<uint8_t, 10> arr;
        logToFile("@arr=%p\n", (void*)arr.data());

        Request * request = Request::fromTLS();
        logToFile("c2\n");
        if (!request) {
            logToFile("[IPC] An error occurred creating the request object (most likely bad header magic)\n");
            return false;
        }

        // Take action based on request type
        bool closeSession = false;
        switch (request->type()) {
            // Call handler to prepare response
            case Request::Type::Request: {
                logToFile("c3\n");
                uint32_t result = this->handler(request);
                logToFile("c4\n");
                request->setResult(result);
                logToFile("c5\n");
                request->toResponseTLS();
                logToFile("c6\n");
                break;
            }

            // Prepare default response
            case Request::Type::Close:
                request->setResult(0);
                request->toResponseTLS();
                closeSession = true;
                break;

            // Otherwise prepare error response
            default:
                logToFile("[IPC] Received unexpected CmifCommand\n");
                request->setResult(MAKERESULT(11, 403));
                request->toResponseTLS();
                break;
        }

        // Send response and delete object
        logToFile("c7\n");
        rc = svcReplyAndReceive(&tmp, &handles[index], 0, handles[index], 0);
        logToFile("c8\n");
        if (rc == KERNELRESULT(TimedOut)) {
            rc = 0;
        }
        delete request;
        logToFile("c9\n");

        // Close session on error or close request
        if (R_FAILED(rc) || closeSession) {
            logToFile("Closing session %i due to error/request\n", index);
            svcCloseHandle(this->handles[index]);
            //this->handles.erase(this->handles.begin() + index);
            handles[index] = 0;
        }

        return (R_SUCCEEDED(rc));
    }

    bool Server::processNewSession() {
        Handle session;
        ::Result rc = svcAcceptSession(&session, this->handles[0]);
        if (R_SUCCEEDED(rc)) {
            // Check we have room
            //if (this->handles.size() >= this->maxHandles) {
            if(handles[1] != 0) {
                logToFile("[IPC] Couldn't handle new session due to limit\n");
                svcCloseHandle(session);

            // Add session to vector
            } else {
                handles[1] = session;
                //this->handles.push_back(session);
            }

            return true;
        }

        return false;
    }

    void Server::setRequestHandler(Handler f) {
        this->handler = f;
    }

    size_t Server::handlesCount() {
        return handles[1] == 0 ? 1 : 2;
    }

    bool Server::process() {
        if (this->error_) {
            return false;
        }

        // Wait for a client to send a request/message
        int32_t handleIndex;
        logToFile("before\n");
        ::Result rc = svcWaitSynchronization(&handleIndex, handles, handlesCount(), waitTimeout);
        logToFile("after\n");
        if (R_VALUE(rc) == KERNELRESULT(TimedOut)) {
            return !this->error_;
        }

        if (R_SUCCEEDED(rc)) {
            // Check we're within range
            if (handleIndex < 0 || static_cast<uint32_t>(handleIndex) >= handlesCount()) {
                logToFile("[IPC] svcWaitSynchronization returned out of range index: %i\n", handleIndex);
                this->error_ = true;
                return false;
            }

            // If the index is not zero then we need to handle that client's request
            bool ok = true;
            if (handleIndex != 0) {
                ok = this->processSession(handleIndex);

            // Otherwise prepare for a new session
            } else {
                ok = this->processNewSession();
            }

            // Exit on an error
            if (!ok) {
                logToFile("[IPC] Failed to handle %s %i request\n", handleIndex == 0 ? "server" : "client ", handleIndex);
                this->error_ = true;
            }
        }

        return !this->error_;
    }

    Server::~Server() {
        // Close all client handles
        /*for (size_t i = 1; i < this->handles.size(); i++) {
            svcCloseHandle(this->handles[i]);
        }

        // Finally close server handle
        if (!this->handles.empty()) {
            svcCloseHandle(this->handles[0]);
            ::Result rc = smUnregisterService(this->serverName);
            if (R_FAILED(rc)) {
                logToFile("[IPC] Couldn't unregister server: %i\n", rc);
            }
        }*/
    }
}