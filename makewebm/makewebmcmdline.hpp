// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include <string>
//#include <limits>
#include <climits>

class CmdLine
{
    CmdLine(const CmdLine&);
    CmdLine& operator=(const CmdLine&);

public:

    CmdLine();

    int Parse(int argc, wchar_t* argv[]);

    const wchar_t* GetInputFileName() const;
    const wchar_t* GetAudioInputFileName() const;
    const wchar_t* GetOutputFileName() const;
    bool ScriptMode() const;
    bool GetList() const;
    bool GetVerbose() const;
    bool GetNoVideo() const;
    bool GetRequireAudio() const;
    bool GetNoAudio() const;
    int GetDeadline() const;
    int GetTargetBitrate() const;
    int GetMinQuantizer() const;
    int GetMaxQuantizer() const;
    int GetUndershootPct() const;
    int GetOvershootPct() const;
    int GetDecoderBufferSize() const;
    int GetDecoderBufferInitialSize() const;
    int GetDecoderBufferOptimalSize() const;
    double GetKeyframeFrequency() const;
    int GetKeyframeMode() const;
    int GetKeyframeMinInterval() const;
    int GetKeyframeMaxInterval() const;
    int GetThreadCount() const;
    int GetErrorResilient() const;
    int GetDropframeThreshold() const;
    int GetResizeAllowed() const;
    int GetResizeUpThreshold() const;
    int GetResizeDownThreshold() const;
    int GetEndUsage() const;
    int GetLagInFrames() const;
    int GetTokenPartitions() const;
    int GetTwoPass() const;
    int GetTwoPassVbrBiasPct() const;
    int GetTwoPassVbrMinsectionPct() const;
    int GetTwoPassVbrMaxsectionPct() const;
    int GetAutoAltRef() const;
    int GetARNRMaxFrames() const;
    int GetARNRStrength() const;
    int GetARNRType() const;
    const wchar_t* GetSaveGraphFile() const;

    static std::wstring GetPath(const wchar_t*);

private:

    const wchar_t* const* m_argv;
    bool m_usage;
    bool m_list;
    bool m_version;
    const wchar_t* m_input;
    const wchar_t* m_audio_input;
    std::wstring m_synthesized_output;
    const wchar_t* m_output;
    bool m_no_video;
    bool m_require_audio;
    bool m_no_audio;

    bool m_script;
    bool m_verbose;
    int m_deadline;
    int m_target_bitrate;
    int m_min_quantizer;
    int m_max_quantizer;
    int m_undershoot_pct;
    int m_overshoot_pct;
    int m_decoder_buffer_size;
    int m_decoder_buffer_initial_size;
    int m_decoder_buffer_optimal_size;
    int m_keyframe_mode;
    int m_keyframe_min_interval;
    int m_keyframe_max_interval;
    double m_keyframe_frequency;
    int m_thread_count;
    int m_dropframe_thresh;
    int m_resize_allowed;
    int m_resize_up_thresh;
    int m_resize_down_thresh;
    int m_error_resilient;
    int m_end_usage;
    int m_lag_in_frames;
    int m_token_partitions;
    int m_two_pass;
    int m_two_pass_vbr_bias_pct;
    int m_two_pass_vbr_minsection_pct;
    int m_two_pass_vbr_maxsection_pct;
    int m_auto_alt_ref;
    int m_arnr_maxframes;
    int m_arnr_strength;
    int m_arnr_type;

    std::wstring m_save_graph_file_str;
    const wchar_t* m_save_graph_file_ptr;

    static bool IsSwitch(const wchar_t*);
    int Parse(wchar_t**);
    int ParseShort(wchar_t**);
    int ParseWindows(wchar_t**);
    int ParseLong(wchar_t**);
    int ParseLongPost(wchar_t**, const wchar_t*, size_t);
    void PrintUsage() const;
    void PrintVersion() const;
    void ListArgs() const;
    void SynthesizeOutput();
    void SynthesizeSaveGraph();

//doesn't compile for some reason
//    enum { kValueIsRequired = std::numeric_limits<int>::min() };

    enum { kValueIsRequired = INT_MAX };

    int ParseOpt(
        wchar_t** i,
        const wchar_t* arg,
        size_t len,
        const wchar_t* name,
        int& value,
        int min,
        int max,
        int optional = kValueIsRequired) const;

};
