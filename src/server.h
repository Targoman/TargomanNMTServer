#include <evhttp.h>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class clsSimpleRestRequest {
private:
  evhttp_request *Request;

public:
  clsSimpleRestRequest(evhttp_request *_request) {
      this->Request = _request;
  };

  evhttp_cmd_type getCommand() const { return evhttp_request_get_command(this->Request); }
  std::string getBody() const {
    evbuffer *Buf = evhttp_request_get_input_buffer(this->Request);
    size_t Len = evbuffer_get_length(Buf);
    std::string Result;
    Result.resize(Len);
    evbuffer_copyout(Buf, (void *)Result.data(), Len);
    return Result;
  }
};

class clsSimpleRestResponse {
private:
  evhttp_request *Request;
  evbuffer *OutBuffer;

public:
  clsSimpleRestResponse(evhttp_request *_request) {
      this->Request = _request;
      this->OutBuffer = evhttp_request_get_output_buffer(_request);
  };

  clsSimpleRestResponse &operator<<(const char *_value) {
    evbuffer_add_printf(this->OutBuffer, "%s", _value);
    return *this;
  }

  clsSimpleRestResponse &operator<<(const char _value) {
    evbuffer_add_printf(this->OutBuffer, "%c", _value);
    return *this;
  }

  clsSimpleRestResponse &operator<<(const int _value) {
    evbuffer_add_printf(this->OutBuffer, "%d", _value);
    return *this;
  }

  clsSimpleRestResponse &operator<<(const float _value) {
    evbuffer_add_printf(this->OutBuffer, "%f", _value);
    return *this;
  }

  clsSimpleRestResponse &operator<<(const std::string &_value) { return (*this << '"' << _value.c_str() << '"'); }

  template<typename T>
  clsSimpleRestResponse &operator<<(const std::vector<T>& _value) {
    evbuffer_add_printf(this->OutBuffer, "[");
    if(_value.size() > 0) {
        this->operator<< (_value[0]);
        for(size_t i = 1; i < _value.size(); ++i) {
            evbuffer_add_printf(this->OutBuffer, ",");
            this->operator<< (_value[i]);
        }
    }
    evbuffer_add_printf(this->OutBuffer, "]");
    return *this;
  }


  void send(int _code = HTTP_OK) { evhttp_send_reply(this->Request, _code, "", this->OutBuffer); }
};

typedef std::function<void(const clsSimpleRestRequest &, clsSimpleRestResponse &)>
    SimpleRestServerCallback_t;

class clsSimpleRestServer {
private:
  std::string Address;
  uint16_t Port;
  int ThreadCount;
  std::atomic_bool Done;

  SimpleRestServerCallback_t Callback;

  void sendErrorResponse(clsSimpleRestResponse &_response, const char *_what) {
    _response << _what;
    _response.send(HTTP_INTERNAL);
  }

  void handleRequest(evhttp_request *_request) {
    const clsSimpleRestRequest Request(_request);
    clsSimpleRestResponse Response(_request);
    try {
      this->Callback(Request, Response);
    } catch(const std::exception &e) {
      this->sendErrorResponse(Response, e.what());
    } catch(...) {
      this->sendErrorResponse(Response, "Unknown exception occurred.");
    }
  }

public:
  clsSimpleRestServer(const char *_address, uint16_t _port, int _threadCount) : Done(false) {
    this->Address = _address;
    this->Port = _port;
    this->ThreadCount = _threadCount;
  }

  void setCallback(SimpleRestServerCallback_t _callback) { this->Callback = _callback; }

  void stop() { this->Done = true; }
  void start();

public:
  friend void handleRequestHelper(evhttp_request *, void *);
};
