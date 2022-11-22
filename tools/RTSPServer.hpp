#ifndef OPENTEXTPROJECTOR_RTSP
#define OPENTEXTPROJECTOR_RTSP

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include "ScreenSource.hpp"

extern "C" {
    #include "x264/x264.h"
}


class OTPRTSPServer {

public:
    OTPRTSPServer(const int port, const int proxyPort):
    m_rtpPortNum(18888), m_rtcpPortNum(m_rtpPortNum+1), m_ttl(255),
    m_rtpPayloadFormat(96), m_estimatedSessionBandwidth(500),
    m_rtspPort(port), m_rtspProxyPort(proxyPort) {
        m_scheduler = BasicTaskScheduler::createNew();
        m_env = BasicUsageEnvironment::createNew(*m_scheduler);
    }

    ~OTPRTSPServer(){
        Medium::close(m_rtspServer);
    }

    bool init(int srcWidth, int srcHeight, int fps, const std::string& streamName) {
        m_srcWidth = srcWidth;
        m_srcHeight = srcHeight;
        m_fps = fps;

        OutPacketBuffer::maxSize = 100000;

        const unsigned maxCNAMElen = 100;
        unsigned char CNAME[maxCNAMElen + 1];
        gethostname((char*)CNAME, maxCNAMElen);
        CNAME[maxCNAMElen] = '\0'; // just in case

        sockaddr_storage destinationAddress;
        destinationAddress.ss_family = AF_INET;
        ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*m_env);
        const Port rtpPort(m_rtpPortNum);
        const Port rtcpPort(m_rtcpPortNum);

        m_rtpGroupSock = new Groupsock(*m_env, destinationAddress, rtpPort, m_ttl);
        m_rtpGroupSock->multicastSendOnly();

        m_rtcpGroupSock = new Groupsock(*m_env, destinationAddress, rtcpPort, m_ttl);
        m_rtcpGroupSock->multicastSendOnly();

        m_videoSink = H264VideoRTPSink::createNew(*m_env, m_rtpGroupSock, m_rtpPayloadFormat);

        m_rtcp = RTCPInstance::createNew(*m_env, m_rtcpGroupSock, m_estimatedSessionBandwidth, CNAME, m_videoSink, nullptr, True);

        m_rtspServer = RTSPServer::createNew(*m_env, m_rtspPort);
        if (m_rtspServer == nullptr) {
            std::cerr << "Failed to create RTSP server: " << m_env->getResultMsg() << std::endl;
            return false;
        }

        ServerMediaSession* sms = ServerMediaSession::createNew(*m_env, streamName.c_str(), "Screen image", "Image from the screen", True);

        sms->addSubsession(PassiveServerMediaSubsession::createNew(*m_videoSink, m_rtcp));

        m_rtspServer->addServerMediaSession(sms);
        announceURL(m_rtspServer, sms);

        return true;
    }

    inline void play() {
        m_screenSource = ScreenSource::createNew(*m_env, m_srcWidth * m_srcHeight, 10);

        if (m_screenSource == nullptr) {
            *m_env << "Failed to create ScreenSource\n";
            exit(1);
        }

        if (!m_screenSource->openEncoder(m_srcWidth, m_srcHeight, m_fps)) {
            *m_env << "Failed to open X264 encoder: \n";
            exit(1);
        }
        m_videoES = m_screenSource;
        m_videoSource = H264VideoStreamFramer::createNew(*m_env, m_videoES);
        std::cout << "Actually playing..." << std::endl;
        m_videoSink->startPlaying(*m_videoSource, nullptr, m_videoSink);
    }

    inline void doEvent(bool& readyToSend, volatile char* watchVar) {
        std::cout << "Playing..." << std::endl;
        readyToSend = true;
        play();
        tthread::this_thread::sleep_for(tthread::chrono::milliseconds(33));
        std::cout << "Doing event loop..." << std::endl;
        m_env->taskScheduler().doEventLoop(watchVar);
        std::cout << "Done doing event loop" << std::endl;
    }

    inline void streamImage(const uint8_t* src, const int index) {
        m_screenSource->encode(src);
    }

    void announceURL(RTSPServer* rtspServer, ServerMediaSession* sms) {
        if (rtspServer == NULL || sms == NULL) return; // sanity check

        UsageEnvironment& env = rtspServer->envir();

        env << "Play this stream using the URL ";
        if (weHaveAnIPv4Address(env)) {
            char* url = rtspServer->ipv4rtspURL(sms);
            env << "\"" << url << "\"";
            delete[] url;
            if (weHaveAnIPv6Address(env)) env << " or ";
        }
        if (weHaveAnIPv6Address(env)) {
            char* url = rtspServer->ipv6rtspURL(sms);
            env << "\"" << url << "\"";
            delete[] url;
        }
        env << "\n";
    }

private:
    // live555
    UsageEnvironment* m_env;
    TaskScheduler* m_scheduler;

    RTSPServer* m_rtspServer;

    const unsigned int m_rtpPortNum;
    const unsigned int m_rtcpPortNum;
    const unsigned char m_ttl;
    Groupsock* m_rtpGroupSock;
    Groupsock* m_rtcpGroupSock;
    const unsigned m_estimatedSessionBandwidth;

    RTPSink* m_videoSink;
    RTCPInstance* m_rtcp;
    FramedSource* m_videoES;
    H264VideoStreamFramer* m_videoSource;

    ScreenSource* m_screenSource;

    // param
    const int m_rtspPort;
    const int m_rtspProxyPort;
    const unsigned char m_rtpPayloadFormat;
    int m_srcWidth;
    int m_srcHeight;
    int m_fps;
};


#endif