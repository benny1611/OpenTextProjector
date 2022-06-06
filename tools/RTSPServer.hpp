#ifndef OPENTEXTPROJECTOR_RTSP
#define OPENTEXTPROJECTOR_RTSP

#include "BasicUsageEnvironment/BasicUsageEnvironment.hh"
#include "liveMedia/liveMedia.hh"
#include "groupsock/GroupsockHelper.hh"
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

    ~OTPRTSPServer(){}

    bool init(int srcWidth, int srcHeight, int fps) {
        m_srcWidth = srcWidth;
        m_srcHeight = srcHeight;
        m_fps = fps;

        OutPacketBuffer::maxSize = 100000;

        int cNameLen = 100;
        m_cName.resize(cNameLen + 1, 0);
        gethostname((char*)&(m_cName[0]), cNameLen);

        m_rtspServer = RTSPServer::createNew(*m_env, m_rtspPort, nullptr);
        if (m_rtspServer == nullptr) {
            std::cerr << "Failed to create RTSP server: " << m_env->getResultMsg() << std::endl;
            return false;
        }

        m_proxyRtspServer = RTSPServerWithREGISTERProxying::createNew(*m_env, m_rtspProxyPort, nullptr, nullptr, 65, true, 2, nullptr,nullptr);
        if (m_proxyRtspServer == nullptr) {
            std::cerr << "Failed to create Proxy RTSP server: " << m_env->getResultMsg() << std::endl;
            return false;
        }

        m_screenSource = ScreenSource::createNew(*m_env, m_srcWidth * m_srcHeight, 10);

        if (!m_screenSource->openEncoder(m_srcWidth, m_srcHeight, m_fps)) {
            std::cerr << "Failed to open X264 encoder: " << std::endl;
            return false;
        }

        return true;
    }

    void addSession(const std::string& streamName) {
        sockaddr_storage destinationAddress;
        ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*m_env);
        const Port rtpPort(m_rtpPortNum);
        const Port rtcpPort(m_rtcpPortNum);

        auto rtpGroupSock = new Groupsock(*m_env, destinationAddress, rtpPort, m_ttl);
        m_rtpGroupSock->multicastSendOnly();

        auto rtcpGroupSock = new Groupsock(*m_env, destinationAddress, rtcpPort, m_ttl);
        m_rtcpGroupSock->multicastSendOnly();

        m_videoSink = H264VideoRTPSink::createNew(*m_env, rtpGroupSock, m_rtpPayloadFormat);

        m_rtcp = RTCPInstance::createNew(*m_env, rtcpGroupSock, m_estimatedSessionBandwidth, &(m_cName[0]), m_videoSink, nullptr, True);

        auto sms = ServerMediaSession::createNew(*m_env, streamName.c_str(), "Screen image", "Image from the screen", True);

        sms->addSubsession(PassiveServerMediaSubsession::createNew(*m_videoSink, m_rtcp));

        auto proxySms = ProxyServerMediaSession::createNew(*m_env, m_proxyRtspServer,
                                                           m_rtspServer->rtspURL(sms), streamName.c_str(),
                                                           nullptr, nullptr, 0, 2);

        m_rtspServer->addServerMediaSession(sms);
        m_proxyRtspServer->addServerMediaSession(proxySms);

        std::cout << "Play this stream using the Local URL: " << m_rtspServer->rtspURL(sms) << std::endl;
        std::cout << "Play this stream using the Proxy URL: " << m_proxyRtspServer->rtspURL(proxySms) << std::endl;
    }

    inline void play() {
        m_videoES = m_screenSource;
        m_videoSource = H264VideoStreamFramer::createNew(*m_env, m_videoES);
        m_videoSink->startPlaying(*m_videoSource, nullptr, nullptr);
    }

    inline void doEvent() {
        std::cout << "Doing event loop..." << std::endl;
        m_env->taskScheduler().doEventLoop();
        std::cout << "Done doing event loop" << std::endl;
    }

    inline void streamImage(const uint8_t* src, const int index) {
        m_screenSource->encode(src);
    }

private:
    // live555
    UsageEnvironment* m_env;
    TaskScheduler* m_scheduler;

    RTSPServer* m_rtspServer;
    RTSPServer* m_proxyRtspServer;

    std::vector<unsigned char> m_cName;
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