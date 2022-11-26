#include <GL/glew.h>
#include <asio/asio.hpp>
#include "RTSPServer.hpp"
#include "tinythread.h"
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

