#include <evhttp.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>

#include "helpers.h"

class clsNmtResponse {
private:
    evhttp_request* request;
    evbuffer* buffer;
    int statusCode_;
public:
    clsNmtResponse(evhttp_request* _request) : request(_request) {
        this->buffer = evhttp_request_get_output_buffer(_request);
    }

public:
    int statusCode() const { return this->statusCode_; };
    void setStatusCode(int _code) { this->statusCode_ = _code; }

    clsNmtResponse& operator << (const nlohmann::json& _json) {
        auto j = _json.dump();
        evbuffer_add(this->buffer, j.data(), j.size());
        return *this;
    }
};

class clsNmtRequest {
private:
    evhttp_request* request;
    clsNmtResponse response_;

public:
    clsNmtRequest(evhttp_request* _request) :
        request(_request),
        response_(clsNmtResponse(_request))
    {}

public:
    evhttp_cmd_type command() const {
        return evhttp_request_get_command(this->request);
    }

    nlohmann::json postBodyAsJson() const {
        auto buf = evhttp_request_get_input_buffer(this->request);
        auto len = evbuffer_get_length(buf);
        std::string rawJson;
        rawJson.resize(len);
        evbuffer_copyout(buf, const_cast<char*>(rawJson.data()), len);
        return nlohmann::json::parse(rawJson);
    }

    clsNmtResponse& response() { return this->response_; }
};

class clsNmtServer {
private:
    std::vector<std::unique_ptr<std::thread>> threads;
    std::atomic_bool stopped;

    typedef std::unique_ptr<event_base, decltype(&event_base_free)> EventBasePtr_t;
    EventBasePtr_t newEventBase() {
        return EventBasePtr_t(event_base_new(), &event_base_free);
    }

    typedef std::unique_ptr<evhttp, decltype(&evhttp_free)> EvHttpPtr_t;
    EvHttpPtr_t newEvHttp(EventBasePtr_t& _eventBase) {
        return EvHttpPtr_t(evhttp_new(_eventBase.get()), &evhttp_free);
    }

    static void __handleRequest(evhttp_request *_request, void *_this_) {
        if(_request == nullptr)
            return;
        clsNmtServer* _this = static_cast<clsNmtServer*>(_this_);
        _this->handleRequest(std::move(clsNmtRequest(_request)));
    }

    void handleRequest(clsNmtRequest&& _request) {
        if(_request.command() != EVHTTP_REQ_POST) {
            nlohmann::json result = {
                {"code", 405},
                {"msg", "Method not allowed."}
            };
            _request.response() << result;
            return;
        }
        auto json = _request.postBodyAsJson();

    }

public:
    clsNmtServer(const std::string& _address, int _port) {
        std::cout << "ASDASDASD" << std::endl;
        evutil_socket_t socket = -1;
        unsigned int threadCount = std::thread::hardware_concurrency() - 1;
        
        std::exception_ptr initializationException;

        for(size_t index = 0; index < threadCount; ++index) {
            std::cout << "Starting thread#" << index << std::endl;
            std::unique_ptr<std::thread> newThread(
                new std::thread(
                    [&] () {
                        try {
                            std::cout << "A" << std::endl;
                            auto eventBase = this->newEventBase();
                            if(!eventBase)
                                throw std::runtime_error("Failed to create a new event base.");
                            std::cout << "B" << std::endl;
                            auto evHttp = this->newEvHttp(eventBase);
                            if(!evHttp)
                                throw std::runtime_error("Failed to create a new http event handler.");
                            std::cout << "C" << std::endl;
                            evhttp_set_gencb(
                                evHttp.get(),
                                clsNmtServer::__handleRequest,
                                this
                            );
                            std::cout << "D" << std::endl;
                            if(socket == -1) {
                                auto boundSocket = evhttp_bind_socket_with_handle(
                                    evHttp.get(),
                                    _address.c_str(),
                                    _port
                                );
                                if(!boundSocket)
                                    throw std::runtime_error("Failed to bind server socket.");
                                socket = evhttp_bound_socket_get_fd(boundSocket);
                                if (socket == -1)
                                    throw std::runtime_error("Failed to get server socket descriptor for next instance.");
                            } else {
                                if (evhttp_accept_socket(evHttp.get(), socket) == -1)
                                    throw std::runtime_error("Failed to bind server socket for new instance.");
                            }
                            std::cout << "E" << std::endl;
                            while(!this->stopped) {
                                event_base_loop(eventBase.get(), EVLOOP_NONBLOCK);
                                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                            }
                        } catch(...) {
                            initializationException = std::current_exception();
                        }
                    }
                )
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (initializationException != std::exception_ptr())
            {
                this->stopped = true;
                std::rethrow_exception(initializationException);
            }
            this->threads.push_back(std::move(newThread));
        }
    }
};