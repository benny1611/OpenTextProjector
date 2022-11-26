#include "TCPServer.h"
#include "base64.h"
#include <fstream>

using namespace std;

void listen_for_connection(void* aArg);
std::string read(tcp::socket& socket);
void send(tcp::socket& socket, const std::string& message);
std::list<std::wstring> getTextFromCommand(json command);
std::vector<std::string> splitVector(std::string strToSplit, char delimeter);

void TCPServer::start() {
    tthread::thread serverThread(listen_for_connection, 0);
    serverThread.detach();
}

void listen_for_connection(void* aArg) {
    cout << "Started listening for commands" << endl;
    while (true) {
        try {
            asio::io_context io_context;

            tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080));

            tcp::socket socket(io_context);

            acceptor.accept(socket);

            for (;;)
            {
                // JSON command
                string message = read(socket);
                json command = json::parse(message);
                //get the font
                try {
                    string font = command["font"].get<string>();
                    ifstream ifile;
                    ifile.open(fontPath + font + ".ttf");
                    if (ifile) { // only change font if the font exists
                        text_mutex.lock();
                        ifile.close();
                        font = fontPath + font + ".ttf";
                        if (string(FONT) != font) {
                            FONT = strcpy((char*)malloc(font.length() + 1), font.c_str());
                            SHOULD_CHANGE_FONT = true;
                        }
                        text_mutex.unlock();
                    }
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the font: " << e.what() << endl;
                }
                // get the padding
                try {
                    int padding = command["padding"].get<int>();
                    text_mutex.lock();
                    PADDING = padding;
                    text_mutex.unlock();
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the padding: " << e.what() << endl;
                }
                // get the font color
                try {
                    vector<float> color = command["font_color"].get<vector<float>>();
                    if (color.size() != 3) {
                        goto skipColor;
                    }
                    float r, g, b;
                    r = color[0];
                    g = color[1];
                    b = color[2];
                    if (r == RED && g == GREEN && b == BLUE) {
                        goto skipColor;
                    }
                    text_mutex.lock();
                    RED = r;
                    GREEN = g;
                    BLUE = b;
                    SHOULD_CHANGE_FONT_COLOR = true;
                    text_mutex.unlock();
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the font color: " << e.what() << endl;
                }
            skipColor:
                // get the text to be shown
                try {
                    list<wstring> result = getTextFromCommand(command);
                    text_mutex.lock();
                    TEXT.clear();
                    for (wstring s : result) {
                        TEXT.push_back(s);
                    }
                    text_mutex.unlock();
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the text: " << e.what() << endl;
                }
                // get the text size
                try {
                    int font_size = command["font_size"].get<int>();
                    text_mutex.lock();
                    TEXT_SCALE = ((float)font_size) / ((float)FONT_SIZE);
                    text_mutex.unlock();
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the font size: " << e.what() << endl;
                }
                try {
                    int monitor = command["monitor"].get<int>();
                    if (monitor < TOTAL_MONITORS && monitor >= 0 && monitor != MONITOR_TO_CHANGE) {
                        monitor_event_mutex.lock();
                        SHOULD_CHANGE_MONITOR = true;
                        MONITOR_TO_CHANGE = monitor;
                        monitor_event_mutex.unlock();
                    }
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the monitor: " << e.what() << endl;
                }
                try {
                    string server = command["server"].get<string>();
                    rtsp_server_mutex.lock();
                    if (server == "start" && !RTSP_SERVER_STARTED) {
                        cout << "Starting the server..." << endl;
                        RTSP_SERVER_STARTED = true;
                        RTSP_SERVER_SHOULD_START = true;
                        watchVariable = NULL;
                    }
                    else if (server == "stop" && RTSP_SERVER_STARTED) {
                        cout << "Closing server ..." << endl;
                        RTSP_SERVER_STARTED = false;
                        watchVariable = 1;
                        readyToSetFrame = false;
                        delete rtspServer;
                    }
                    else {
                        cerr << "Error: command \"" << server << "\" not valid. Is server currently running: " << RTSP_SERVER_STARTED << endl;
                    }
                    rtsp_server_mutex.unlock();
                }
                catch (exception& e) {
                    cerr << "Error while trying to get the server command: " << e.what() << endl;
                }
                //screenshot_mutex.unlock();
            }
        }
        catch (exception& e) {
            cerr << "Another exception has occurred: " << e.what() << endl;
        }
    }
    cout << "Done listening for commands" << endl;
}

std::string read(tcp::socket& socket) {
    asio::streambuf buf;
    asio::read_until(socket, buf, "\n");
    auto data = asio::buffer_cast<const char*>(buf.data());
    string s(reinterpret_cast<char const*>(data));
    string result = "";
    return s;
}

void send(tcp::socket& socket, const std::string& message) {
    const string msg = message + "\n";
    asio::write(socket, asio::buffer(message));
}

std::list<std::wstring> getTextFromCommand(json command) {
    string base64String = command["text"].get<string>();
    string result64 = base64_decode(base64String, false);
    vector<string> text_list = splitVector(result64, '\n');
    list<wstring> results;
    for (string t : text_list) {
        wstring result = L"";
        for (int i = 0; i < t.length(); i++) {
            if ((unsigned int)t[i] == 4294967236 && (unsigned int)t[i + 1] == 4294967171) {
                result += L"ă";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967214) {
                result += L"î";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967202) {
                result += L"â";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967240 && (unsigned int)t[i + 1] == 4294967193) {
                result += L"ș";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967240 && (unsigned int)t[i + 1] == 4294967195) {
                result += L"ț";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967240 && (unsigned int)t[i + 1] == 4294967194) {
                result += L"Ț";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967240 && (unsigned int)t[i + 1] == 4294967192) {
                result += L"Ș";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967170) {
                result += L"Â";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967236 && (unsigned int)t[i + 1] == 4294967170) {
                result += L"Ă";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967182) {
                result += L"Î";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967190) {
                result += L"Ö";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967222) {
                result += L"ö";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967172) {
                result += L"Ä";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967204) {
                result += L"ä";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967196) {
                result += L"Ü";
                i++;
            }
            else if ((unsigned int)t[i] == 4294967235 && (unsigned int)t[i + 1] == 4294967228) {
                result += L"ü";
                i++;
            }
            else {
                result += t[i];
            }
        }
        results.push_back(result);
    }
    return results;
}

std::vector<std::string> splitVector(std::string strToSplit, char delimeter) {
    stringstream ss(strToSplit);
    string item;
    vector<string> splittedStrings;
    while (getline(ss, item, delimeter))
    {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}
