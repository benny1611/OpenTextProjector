/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2021 Live Networks, Inc.  All rights reserved.
// A template for a MediaSource encapsulating an audio/video input device
//
// NOTE: Sections of this code labeled "%%% TO BE WRITTEN %%%" are incomplete, and need to be written by the programmer
// (depending on the features of the particular device).
// Implementation

#include "ScreenSource.hh"
#include <groupsock/GroupsockHelper.hh> // for "gettimeofday()"
#include <iostream>

#ifdef __cplusplus
extern "C" {
    #include "libavcodec/avcodec.h"
    #include "libavutil/frame.h"
    #include "libavutil/imgutils.h"
    #include "libswscale/swscale.h"
} // endof extern "C"
#endif

void ffmpeg_encoder_start(AVCodecID codec_id, int fps, int width, int height);

EventTriggerId ScreenSource::eventTriggerId = 0;
tthread::mutex ScreenSource::frameMutex;

unsigned ScreenSource::referenceCount = 0;
static AVCodecContext *c = nullptr;
static AVFrame *frame;
static AVPacket pkt;
struct SwsContext *sws_context = nullptr;

ScreenSource*
ScreenSource::createNew(UsageEnvironment& env,
                        int height, int width,
                        int playTimePerFrame) {
  return new ScreenSource(env, height, width, playTimePerFrame);
}

ScreenSource::ScreenSource(UsageEnvironment& env, int height,
                           int width, int playTimePerFrame)
        : FramedSource(env),
        frameHeight(height),
        frameWidth(width),
        fPlayTimePerFrame(playTimePerFrame),
        fLastPlayTime(0) {
    if (referenceCount == 0) {
        // Any global initialization of the device would be done here:
        std::cout << "Starting to serve screenshots ..." << std::endl;
    }
    ++referenceCount;

    std::cout << "Setting frame width and height and starting the encoder" << std::endl;
    ffmpeg_encoder_start(AV_CODEC_ID_H264, 10, width, height);

    if (eventTriggerId == 0) {
        std::cout << "Event trigger done! ..." << std::endl;
        eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    }
    std::cout << "Is this still awaiting data (constructor): " << (bool)isCurrentlyAwaitingData() << std::endl;
}

ScreenSource::~ScreenSource() {
    // Any instance-specific 'destruction' (i.e., resetting) of the device would be done here:
    //%%% TO BE WRITTEN %%%

    --referenceCount;
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;
}

static void ffmpeg_encoder_set_frame_yuv_from_rgb(const uint8_t *rgb) {
    const int in_linesize[1] = { (3 * c->width) };
    int ret;

    frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width  = c->width;
    frame->height = c->height;
    ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height, c->pix_fmt, 32);
    if (ret < 0) {
        std::cerr << "Could not allocate raw picture buffer" << std::endl;
        exit(1);
    }
    const uint8_t* const src_data[1] = { rgb };
    std::cout << "Scaling" << std::endl;
    sws_scale(sws_context, src_data, in_linesize, 0,
              c->height, frame->data, frame->linesize);  // this line can get tricky
    std::cout << "Done scaling" << std::endl;
    delete rgb;
}

/* Allocate resources and write header data to the output file. */
void ffmpeg_encoder_start(AVCodecID codec_id, int fps, int width, int height) {
    const AVCodec *codec;
    codec = avcodec_find_encoder(codec_id);
    if (!codec ) {
        std::cerr << "Codec not found" << std::endl;
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        exit(1);
    }
    c->bit_rate = 400000;
    c->width = width;
    c->height = height;
    c->time_base.num = 1;
    c->time_base.den = fps;
    c->keyint_min = 600;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    if (avcodec_open2(c, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        exit(1);
    }
    if (sws_context == nullptr) {
        sws_context = sws_getContext(c->width, c->height, AV_PIX_FMT_RGB24, c->width,
                                     c->height, AV_PIX_FMT_YUVA420P, SWS_FAST_BILINEAR,
                                     nullptr, nullptr, nullptr);
        /*sws_context = sws_getCachedContext(sws_context,
            c->width, c->height, AV_PIX_FMT_RGB24,
            c->width, c->height, AV_PIX_FMT_YUV420P,
            0, 0, 0, 0);*/
    }
}

/*
Write trailing data to the output file
and free resources allocated by ffmpeg_encoder_start.
*/
void ffmpeg_encoder_finish(unsigned char* fTo) {
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    int got_output, error;
    do {
        fflush(stdout);

        error = avcodec_send_frame(c, NULL);

        if ( error != AVERROR_EOF && error != AVERROR(EAGAIN) && error != 0){
            std::cerr << "Error encoding frame" << std::endl;
            exit(1);
        }
        if ( (error = avcodec_receive_packet(c, &pkt)) == 0) {
            got_output = 1;
        }
        if (got_output) {
            memmove(fTo, pkt.data, pkt.size);
            //fwrite(pkt.data, 1, pkt.size, file);
            av_packet_unref(&pkt);
        }
    } while (got_output);
    memmove(fTo, endcode, sizeof(endcode));
    avcodec_close(c);
    av_free(c);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
}

/*
Encode one frame from an RGB24 input and save it to the output file.
Must be called after ffmpeg_encoder_start, and ffmpeg_encoder_finish
must be called after the last call to this function.
*/
void ffmpeg_encoder_encode_frame(uint8_t *rgb, unsigned char* fTo) {
    int error;
    ffmpeg_encoder_set_frame_yuv_from_rgb(rgb);
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    if (frame->pts == 1) {
        frame->key_frame = 1;
        frame->pict_type = AV_PICTURE_TYPE_I;
    } else {
        frame->key_frame = 0;
        frame->pict_type = AV_PICTURE_TYPE_P;
    }
    bool got_output = false;

    while (!got_output) {
        error = avcodec_send_frame(c, frame);
        if (error != AVERROR_EOF && error != AVERROR(EAGAIN) && error != 0){
            std::cerr << "Error while encoding frame" << std::endl;
            exit(1);
        }
        error = avcodec_receive_packet(c, &pkt);
        got_output = error == 0;
        if (error != -11) {
            if (error != 0) {
                std::cerr << "avcodec_receive_packet got another error: " << error << std::endl;
            }
            break;
        }
    }

    std::cout << "Got output? " << got_output << " here's the error: " << error << std::endl;
    if (got_output) {
        std::cout << "Copying output..." << std::endl;
        memmove(fTo, pkt.data, pkt.size);
        av_packet_unref(&pkt);
    }
    std::cout << "Done encoding the frame" << std::endl;
}

void ScreenSource::doGetNextFrame() {
    std::cout << "doGetNextFrame! ..." << std::endl;
    deliverFrame();
}

void ScreenSource::deliverFrame0(void* clientData) {
    frameMutex.lock();
    std::cout << "deliverFrame0! ..." << std::endl;
    ((ScreenSource*)clientData)->deliverFrame();
    std::cout << "deliverFrame0 - Done!" << std::endl;
    frameMutex.unlock();
    //envir().taskScheduler().triggerEvent(eventTriggerId, this);
}

void ScreenSource::setNextFrame(BYTE* screenshot, int width, int height) {
    nextFramePixels = screenshot;
    this->frameWidth = width;
    this->frameHeight = height;
    envir().taskScheduler().triggerEvent(eventTriggerId, this);
}

void ScreenSource::deliverFrame() {

    if (!isCurrentlyAwaitingData()) {
        std::cout << "Not ready yet sorry" << std::endl;
        return; // we're not ready for the data yet
    }
    if(frameWidth == 0 || frameHeight == 0 || nextFramePixels == nullptr) {
        std::cout << "Not ready yet sorry, no frames were set" << std::endl;
        return;
    }
    std::cout << "Delivering... " << frameWidth << " X " << frameHeight << std::endl;

    //std::cout << "Done getting the lock :)" << std::endl;
    if (fPlayTimePerFrame > 0 && fMaxSize > 0) {
        if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
            gettimeofday(&fPresentationTime, nullptr);
        } else {
            unsigned uSeconds = fPresentationTime.tv_usec + fLastPlayTime;
            fPresentationTime.tv_sec += uSeconds / 1000000;
            fPresentationTime.tv_usec = uSeconds % 1000000;
        }

        fLastPlayTime = (fPlayTimePerFrame * fFrameSize) / fMaxSize;
        fDurationInMicroseconds = fLastPlayTime;
    } else {
        gettimeofday(&fPresentationTime, nullptr);
    }
    ffmpeg_encoder_encode_frame(nextFramePixels, fTo);
    FramedSource::afterGetting(this);
}
