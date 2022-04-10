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

ScreenSource*
ScreenSource::createNew(UsageEnvironment& env) {
  return new ScreenSource(env);
}

EventTriggerId ScreenSource::eventTriggerId = 0;

unsigned ScreenSource::referenceCount = 0;
static AVCodecContext *c = NULL;
static AVFrame *frame;
static AVPacket pkt;
struct SwsContext *sws_context = NULL;

static void ffmpeg_encoder_set_frame_yuv_from_rgb(uint8_t *rgb) {
    const int in_linesize[1] = { 3 * c->width };
    sws_context = sws_getCachedContext(sws_context,
            c->width, c->height, AV_PIX_FMT_RGB24,
            c->width, c->height, AV_PIX_FMT_YUV420P,
            0, 0, 0, 0);
    const uint8_t* const src_data[] = { rgb };
    std::cout << "Sws_scale output: "  << std::endl;
    sws_scale(sws_context, src_data, in_linesize, 0,
              c->height, frame->data, frame->linesize);
    std::cout << "Done!" << std::endl;
}

/* Allocate resources and write header data to the output file. */
void ffmpeg_encoder_start(AVCodecID codec_id, int fps, int width, int height) {
    const AVCodec *codec;
    int ret;
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
    if (avcodec_open2(c, codec, NULL) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        exit(1);
    }
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
    //fwrite(endcode, 1, sizeof(endcode), file);
    //fclose(file);
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
    std::cout << "Here it comes ..." << std::endl;
    int got_output, error;
    std::cout << "1" << std::endl;
    ffmpeg_encoder_set_frame_yuv_from_rgb(rgb);
    std::cout << "2" << std::endl;
    av_init_packet(&pkt);
    std::cout << "3" << std::endl;
    pkt.data = NULL;
    pkt.size = 0;
    if (frame->pts == 1) {
        frame->key_frame = 1;
        frame->pict_type = AV_PICTURE_TYPE_I;
    } else {
        frame->key_frame = 0;
        frame->pict_type = AV_PICTURE_TYPE_P;
    }

    error = avcodec_send_frame(c, frame);
    if (error != AVERROR_EOF && error != AVERROR(EAGAIN) && error != 0){
        std::cerr << "Error while encoding frame" << std::endl;
        exit(1);
    }
    if ( (error = avcodec_receive_packet(c, &pkt)) == 0) {
        got_output = 1;
    }
    std::cout << "Got output? " << got_output << std::endl;
    if (got_output) {
        std::cout << "Copying output..." << std::endl;
        memmove(fTo, pkt.data, pkt.size);
        av_packet_unref(&pkt);
    }
}



ScreenSource::ScreenSource(UsageEnvironment& env)
  : FramedSource(env) {
  if (referenceCount == 0) {
    // Any global initialization of the device would be done here:
    std::cout << "Starting to serve screenshots ..." << std::endl;
  }
  ++referenceCount;

  // Any instance-specific initialization of the device would be done here:
    /* find the mpeg1video encoder */
  //codec = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
  // We arrange here for our "deliverFrame" member function to be called
  // whenever the next frame of data becomes available from the device.
  //
  // If the device can be accessed as a readable socket, then one easy way to do this is using a call to
  //     envir().taskScheduler().turnOnBackgroundReadHandling( ... )
  // (See examples of this call in the "liveMedia" directory.)
  //
  // If, however, the device *cannot* be accessed as a readable socket, then instead we can implement it using 'event triggers':
  // Create an 'event trigger' for this device (if it hasn't already been done):
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

void ScreenSource::doGetNextFrame() {
    std::cout << "doGetNextFrame! ..." << std::endl;
    std::cout << "Is this still awaiting data (doGetNextFrame): " << (bool)isCurrentlyAwaitingData() << std::endl;
    deliverFrame();
}

void ScreenSource::deliverFrame0(void* clientData) {
    std::cout << "deliverFrame0! ..." << std::endl;
    ((ScreenSource*)clientData)->deliverFrame();
    //envir().taskScheduler().triggerEvent(eventTriggerId, this);
}

void ScreenSource::setNextFrame(BYTE* screenshot, int width, int height) {
    bool startEncoder = frameWidth == 0 && frameHeight == 0;
    if(startEncoder) {
        std::cout << "Setting frame width and height and starting the encoder" << std::endl;
        ffmpeg_encoder_start(AV_CODEC_ID_H264, 100, width, height);
    }

    nextFramePixels = screenshot;
    frameWidth = width;
    frameHeight = height;
    if (startEncoder) {
        envir().taskScheduler().triggerEvent(eventTriggerId, this);
    }
}

void ScreenSource::deliverFrame() {
    if (!isCurrentlyAwaitingData()) {
        std::cout << "Not ready yet sorry" << std::endl;
        return; // we're not ready for the data yet
    }
    if(frameWidth == 0 && frameHeight == 0) {
        std::cout << "Not ready yet sorry, no frames were set" << std::endl;
        return;
    }
    std::cout << "Delivering..." << std::endl;
    frameMutex.lock();
    ffmpeg_encoder_encode_frame(nextFramePixels, fTo);
    frameMutex.unlock();
}
