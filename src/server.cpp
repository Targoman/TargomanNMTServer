#include "server.h"
#include <cstring>
#include <memory>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <marian/common/logging.h>
#pragma GCC diagnostic pop

void handleRequestHelper(evhttp_request* _request, void* _this) {
  const auto This = static_cast<clsSimpleRestServer*>(_this);
  This->handleRequest(_request);
}

void clsSimpleRestServer::start() {
  std::setvbuf(stdout, NULL, _IONBF, 0);
  std::setvbuf(stdin, NULL, _IONBF, 0);

  auto ThreadDeleter = [&](std::thread* _thread) {
    Done = true;
    _thread->join();
    delete _thread;
  };

  typedef std::unique_ptr<std::thread, decltype(ThreadDeleter)> ThreadPtr_t;
  typedef std::vector<ThreadPtr_t> ThreadPool_t;

  std::exception_ptr InitException;
  evutil_socket_t Socket = -1;

  auto bindEvHttp = [&](evhttp* _evHttp) {
    if(Socket == -1) {
      evhttp_bound_socket* BoundSock
          = evhttp_bind_socket_with_handle(_evHttp, this->Address.c_str(), this->Port);
      if(!BoundSock)
        throw std::runtime_error("Failed to bind server socket.");
      Socket = evhttp_bound_socket_get_fd(BoundSock);
      if(Socket == -1)
        throw std::runtime_error("Failed to get server socket descriptor for next instance.");
    } else {
      if(evhttp_accept_socket(_evHttp, Socket) == -1)
        throw std::runtime_error("Failed to bind server socket for new thread.");
    }
  };

  auto runEventLoop = [&]() {
    std::unique_ptr<event_base, decltype(&event_base_free)> EventBase(event_base_new(),
                                                                      &event_base_free);
    if(!EventBase)
      throw std::runtime_error("Failed to create a new event base.");
    std::unique_ptr<evhttp, decltype(&evhttp_free)> EvHttp(evhttp_new(EventBase.get()),
                                                           &evhttp_free);
    if(!EvHttp)
      throw std::runtime_error("Failed to create a new http event handler.");
    evhttp_set_gencb(EvHttp.get(), handleRequestHelper, static_cast<void*>(this));
    bindEvHttp(EvHttp.get());
    while(Done == false) {
      event_base_loop(EventBase.get(), EVLOOP_NONBLOCK);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  Done = false;

  ThreadPool_t Pool;
  int ThreadCount
      = this->ThreadCount == -1 ? static_cast<int>(std::thread::hardware_concurrency() - 1) : this->ThreadCount;
  for(int Index = 0; Index < ThreadCount; ++Index) {
    ThreadPtr_t Thread(new std::thread([&]() {
                         try {
                           runEventLoop();
                         } catch(...) {
                           InitException = std::current_exception();
                         }
                       }),
                       ThreadDeleter);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if(InitException != std::exception_ptr()) {
      Done = true;
      std::rethrow_exception(InitException);
    }
    Pool.push_back(std::move(Thread));
  }

  LOG(info, "[rest_server] Rest server started. Listening on {} ...", this->Port);
  while(!Done)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ;
}
