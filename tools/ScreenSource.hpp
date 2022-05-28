#ifndef OPENTEXTPROJECTOR_SCREENSOURCE
#define OPENTEXTPROJECTOR_SCREENSOURCE

extern "C" {
#include "x264/x264.h"
}

#include "liveMedia/FramedSource.hh"
#include "concurrent_queue.hpp"
#include "encoder.hpp"

class ScreenSource: public FramedSource {
public:
    static std::vector<ScreenSource*> createNew(UsageEnvironment& env, unsigned preferredFrameSize, unsigned playTimePerFrame, const int sessionNum) {
        std::vector<ScreenSource*> screenSourceArray;
        for (auto i = 0; i < sessionNum; i++) {
            screenSourceArray.push_back(new ScreenSource(env, preferredFrameSize, playTimePerFrame));
        }
        return screenSourceArray;
    }

    bool openEncoder(int srcWidth, int srcHeight, int fps) {
        if (!m_encoder.open(srcWidth, srcHeight, fps)) {
            return false;
        } else {
            return true;
        }
    }

    void encode(const uint8_t* src) {
        m_encoder.encoding(src);

        envir().taskScheduler().triggerEvent(m_eventTriggerId, this);
    }

protected:
    ScreenSource(UsageEnvironment& env, unsigned preferredFrameSize, unsigned playTimePerFrame):
    FramedSource(env),
    fPreferredFrameSize(fMaxSize),
    fPlayTimePerFrame(playTimePerFrame),
    fLastPlayTime(0),
    m_encoder(Encoder(m_buffer)),
    m_buffer() {
        ++m_referenceCount;
        if (m_eventTriggerId == 0) {
            m_eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
        }
    }

private:
    virtual void doGetNextFrame(){
        deliverFrame();
    }

    static void deliverFrame0(void* clientData) {
        ((ScreenSource*)clientData)->deliverFrame();
    }
    void deliverFrame(){
        if(!isCurrentlyAwaitingData()) {
            return;
        }

        if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
            if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
                gettimeofday(&fPresentationTime, nullptr);
            }
            else {
                unsigned uSeconds	= fPresentationTime.tv_usec + fLastPlayTime;
                fPresentationTime.tv_sec += uSeconds / 1000000;
                fPresentationTime.tv_usec = uSeconds % 1000000;
            }

            fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
            fDurationInMicroseconds = fLastPlayTime;
        }
        else {
            gettimeofday(&fPresentationTime, nullptr);
        }

        if(!m_buffer.empty()) {
            m_buffer.pop(m_nalToDeliver);

            unsigned newFrameSize = m_nalToDeliver.i_payload;

            if (newFrameSize > fMaxSize) {
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = newFrameSize - fMaxSize;
            }
            else {
                fFrameSize = newFrameSize;
            }

            auto res = std::copy(m_nalToDeliver.p_payload, m_nalToDeliver.p_payload + m_nalToDeliver.i_payload, fTo);

            FramedSource::afterGetting(this);
        }
    }

private:
    unsigned fPreferredFrameSize;
    unsigned fPlayTimePerFrame;
    unsigned fNumSources;
    unsigned fCurrentlyReadSourceNumber;
    unsigned fLastPlayTime;

    Encoder m_encoder;

    x264_nal_t m_nalToDeliver;
    concurrent_queue<x264_nal_t> m_buffer;

    unsigned m_referenceCount;
    timeval m_currentTime;

    EventTriggerId m_eventTriggerId;
};

#endif