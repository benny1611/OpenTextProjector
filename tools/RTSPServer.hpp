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
    OTPRTSPServer(const int port, const int proxyPort, const int sessionNum):
    m_rtpPortNum(18888), m_rtcpPortNum(m_rtpPortNum+1), m_ttl(255),
    m_rtpPayloadFormat(96), m_estimatedSessionBandwidth(500),
    m_rtspPort(port), m_rtspProxyPort(proxyPort), m_sessionNum(sessionNum) {
        m_scheduler = BasicTaskScheduler::createNew();
        m_env = BasicUsageEnvironment::createNew(reinterpret_cast<TaskScheduler &>(m_scheduler));
        m_authDB = UserAuthenticationDatabase(nullptr);
        m_videoSink = std::vector<RTPSink*>(m_sessionNum, nullptr);
        m_rtcp = std::vector<RTCPInstance*>(m_sessionNum, nullptr);
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

        m_rtspServer = RTSPServer::createNew(*m_env, m_rtspPort, &m_authDB);
        if (m_rtspServer == nullptr) {
            std::cerr << "Failed to create RTSP server: " << m_env->getResultMsg() << std::endl;
            return false;
        }

        m_proxyRtspServer = RTSPServerWithREGISTERProxying::createNew(*m_env, m_rtspProxyPort, &m_authDB, nullptr, 65, true, 2, nullptr,nullptr);
        if (m_proxyRtspServer == nullptr) {
            std::cerr << "Failed to create Proxy RTSP server: " << m_env->getResultMsg() << std::endl;
            return false;
        }

        m_screenSource = ScreenSource::createNew(*m_env, 0, 0, m_sessionNum);
        for (auto i = 0; i < m_sessionNum; i++) {
            if (!m_screenSource[i]->openEncoder(srcWidth, srcHeight, fps)) {
                std::cerr << "Failed to open X264 encoder: " << i << std::endl;
                return false;
            }
        }

        return true;
    }

    void addSession(const std::string& streamName, const int index) {
        sockaddr_storage destinationAddress;
        ((struct sockaddr_in&)destinationAddress).sin_addr.s_addr = chooseRandomIPv4SSMAddress(*m_env);
        const Port rtpPort(m_rtpPortNum + (index * 2));
        const Port rtcpPort(m_rtcpPortNum + (index * 2));

        auto rtpGroupSock = new Groupsock(*m_env, destinationAddress, rtpPort, m_ttl);
        m_rtpGroupSock->multicastSendOnly();

        auto rtcpGroupSock = new Groupsock(*m_env, destinationAddress, rtpPort, m_ttl);
        m_rtcpGroupSock->multicastSendOnly();

        m_videoSink[index] = H264VideoRTPSink::createNew(*m_env, rtpGroupSock, m_rtpPayloadFormat);

        m_rtcp[index] = RTCPInstance::createNew(*m_env, rtcpGroupSock, m_estimatedSessionBandwidth, &(m_cName[0]), m_videoSink[index], nullptr, True);

        auto sms = ServerMediaSession::createNew(*m_env, streamName.c_str(), "Screen image", "Image from the screen", True);

        sms->addSubsession(PassiveServerMediaSubsession::createNew(*m_videoSink[index], m_rtcp[index]));

        auto proxySms = ProxyServerMediaSession::createNew(*m_env, m_proxyRtspServer,
                                                           m_rtspServer->rtspURL(sms), streamName.c_str(),
                                                           nullptr, nullptr, 0, 1);

        m_rtspServer->addServerMediaSession(sms);
        m_proxyRtspServer->addServerMediaSession(proxySms);

        std::cout << "Play this stream using the Local URL: " << m_rtspServer->rtspURL(sms) << std::endl;
        std::cout << "Play this stream using the Proxy URL %s" << m_proxyRtspServer->rtspURL(proxySms) << std::endl;
    }

    inline void play(const int index) {
        m_videoES = m_screenSource[index];
        m_videoSource = H264VideoStreamFramer::createNew(*m_env, m_videoES);
        m_videoSink[index]->startPlaying(*m_videoSource, nullptr, nullptr);
    }

    inline void doEvent() {
        m_env->taskScheduler().doEventLoop();
    }

    inline void streamImage(const uint8_t* src, const int index) {
        m_screenSource[index]->encode(src);
    }

private:
    // live555
    UsageEnvironment* m_env;
    TaskScheduler* m_scheduler;
    UserAuthenticationDatabase m_authDB;

    RTSPServer* m_rtspServer;
    RTSPServer* m_proxyRtspServer;

    std::vector<unsigned char> m_cName;
    const unsigned int m_rtpPortNum;
    const unsigned int m_rtcpPortNum;
    const unsigned char m_ttl;
    Groupsock* m_rtpGroupSock;
    Groupsock* m_rtcpGroupSock;
    const unsigned m_estimatedSessionBandwidth;

    std::vector<RTPSink*> m_videoSink;
    std::vector<RTCPInstance*> m_rtcp;
    ServerMediaSession* m_sms;
    FramedSource* m_videoES;
    H264VideoStreamFramer* m_videoSource;

    std::vector<ScreenSource*> m_screenSource;

    // param
    const int m_rtspPort;
    const int m_rtspProxyPort;
    const unsigned char m_rtpPayloadFormat;
    const int m_sessionNum;
    int m_srcWidth;
    int m_srcHeight;
    int m_fps;
};


#endif