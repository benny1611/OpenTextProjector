//
// Created by Benny on 26-May-22.
//

#ifndef OPENTEXTPROJECTOR_ENCODER_HPP
#define OPENTEXTPROJECTOR_ENCODER_HPP

#include <iostream>
#include "concurrent_queue.hpp"
#ifdef __cplusplus
extern "C" {
#endif
    #include "x264/x264.h"
    #include "libswscale/swscale.h"
}


class Encoder {
public:
    Encoder(concurrent_queue<x264_nal_t>& buffer): m_encoder(nullptr), m_numNals(0), m_pts(0), m_buffer(buffer){}

    ~Encoder() {
        x264_encoder_close(m_encoder);
        sws_freeContext(m_sws);
    }

    bool open(int srcWidth, int srcHeight, int fps) {
        x264_param_default_preset(&m_x264Params, "ultrafast", "zerolatency");
        m_x264Params.i_log_level = X264_LOG_ERROR;

        m_x264Params.i_threads = 2;
        m_x264Params.i_width = srcWidth;
        m_x264Params.i_height = srcHeight;
        m_x264Params.i_fps_num = fps;
        m_x264Params.i_fps_den = 1;
        m_x264Params.i_csp = X264_CSP_I420;

        m_x264Params.i_keyint_max = fps;
        m_x264Params.b_intra_refresh = 1;

        m_x264Params.rc.i_rc_method = X264_RC_CRF;
        m_x264Params.rc.i_vbv_buffer_size = 100;
        m_x264Params.rc.i_vbv_max_bitrate = 1000;
        m_x264Params.rc.f_rf_constant = 25;
        m_x264Params.rc.f_rf_constant_max = 35;
        m_x264Params.i_sps_id = 7;

        m_x264Params.b_repeat_headers = 1;
        m_x264Params.b_annexb = 1;

        x264_param_apply_profile(&m_x264Params, "baseline");

        m_in.i_type = X264_TYPE_AUTO;
        m_in.img.i_csp = X264_CSP_I420;


        if(m_encoder){
            std::cerr << "Already opened. first call close()" << std::endl;
            return false;
        }

        m_encoder = x264_encoder_open(&m_x264Params);
        if (!m_encoder){
            std::cerr << "Cannot open the encoder" << std::endl;
            return false;
        }

        if(x264_picture_alloc(&m_in, m_x264Params.i_csp, m_x264Params.i_width, m_x264Params.i_height)){
            std::cerr << "Cannot allocate x264 picure" << std::endl;
            return false;
        }

        // create sws handle
        m_sws = sws_getContext(m_x264Params.i_width, m_x264Params.i_height, m_inFormat, m_x264Params.i_width, m_x264Params.i_height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                               nullptr, nullptr, nullptr);
        if (!m_sws){
            std::cerr << "Cannot create SWS context" << std::endl;
            return false;
        }

        return true;
    }

    bool encoding(const uint8_t* src) {
        // scale change
        {
            int srcStride = m_x264Params.i_width * 3;

            if (!m_sws){
                std::cerr << "Not initialized, so cannot encode" << std::endl;
                return false;
            }

            int h = sws_scale(m_sws, &src, &srcStride, 0, m_x264Params.i_height, m_in.img.plane, m_in.img.i_stride);
            if (h != m_x264Params.i_height){
                std::cerr << "scale failed: " << h << std::endl;
                return false;
            }

            m_in.i_pts = m_pts;
        }

        // encode to h264
        {
            int frame_size = x264_encoder_encode(m_encoder, &m_nals, &m_numNals, &m_in, &m_out);
            if(frame_size){

                static bool alreadydone = false;
                if(!alreadydone){
                    x264_encoder_headers(m_encoder, &m_nals, &m_numNals);
                    alreadydone = true;
                }

                for(int i = 0 ; i < m_numNals ; i++)
                    m_buffer.push(m_nals[i]);
            }
            else
                return false;

            m_pts++;
        }
        //std::cout << "Pushed " << m_numNals << " nals in the queue" << std::endl;

        return true;
    }

private:
    // x264
    AVPixelFormat m_inFormat = AV_PIX_FMT_RGB24;

    x264_param_t m_x264Params;
    x264_t* m_encoder;
    x264_nal_t* m_nals;

    x264_picture_t m_in;
    x264_picture_t m_out;

    SwsContext* m_sws;

    int m_numNals;
    int m_pts;

    // buffer
    concurrent_queue<x264_nal_t>& m_buffer;
};


#endif //OPENTEXTPROJECTOR_ENCODER_HPP
