#ifndef OPENTEXTPROJECTOR_SCREENSOURCE
#define OPENTEXTPROJECTOR_SCREENSOURCE

extern "C" {
#include "x264/x264.h"
}

#include "FramedSource.hh"
#include "GroupsockHelper.hh"
#include "encoder.hpp"

class ScreenSource: public FramedSource {
public:
    static ScreenSource* createNew(UsageEnvironment& env, unsigned preferredFrameSize, unsigned playTimePerFrame) {
        return new ScreenSource(env, preferredFrameSize, playTimePerFrame);
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
    fPreferredFrameSize(preferredFrameSize),
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
            //std::cout << "Here is the size of the image: " << m_nalToDeliver.i_payload << std::endl;
            //std::copy(m_nalToDeliver.p_payload, m_nalToDeliver.p_payload + m_nalToDeliver.i_payload, fTo);
            memmove(fTo, m_nalToDeliver.p_payload, m_nalToDeliver.i_payload);

            FramedSource::afterGetting(this);
        }
    }

private:
    unsigned fPreferredFrameSize;
    unsigned fPlayTimePerFrame;
    unsigned fLastPlayTime;

    Encoder m_encoder;

    x264_nal_t m_nalToDeliver;
    concurrent_queue<x264_nal_t> m_buffer;

    unsigned m_referenceCount;

    EventTriggerId m_eventTriggerId;
};

#endif