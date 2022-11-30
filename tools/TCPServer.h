#include <asio/ip/tcp.hpp>
#include "json.hpp"
#include "../main.h"

using asio::ip::tcp;
using json = nlohmann::json;

class TCPServer {
public:
	inline TCPServer(int port) {
		this->port = port;
	}

	void start();

private:
	int port;
};

