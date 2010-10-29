/*
 *  wfxrawreader.cpp
 *  sdlaudioplayer
 *
 *  Created by Tom Finegan on 10/24/10.
 *  Copyright 2010 WebM Project. All rights reserved.
 *
 */

#include <cassert>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "mferror.h"
#include "windows.h"
#include "mmsystem.h"
#include "mmreg.h"
#endif

#include "wfxrawreader.hpp"

namespace WebmMfVorbisDecLib
{

const int kWfxRawHeaderSize = 3;

WfxRawReader::WfxRawReader() : file_bytes_read_(0), file_size_(0)
{
    ::memset(&wave_format_, 0, sizeof(WAVEFORMATEX));
}

WfxRawReader::~WfxRawReader()
{
    Close();
}

int WfxRawReader::CheckHeader_()
{
    int result = E_FAIL;
    char header_buf[3];

    assert(wave_extra_.size() > kWfxRawHeaderSize);

    for (int i = 0; i < kWfxRawHeaderSize; ++i)
        header_buf[i] = wave_extra_[i];

    wave_extra_.erase(wave_extra_.begin(),
                      wave_extra_.begin()+kWfxRawHeaderSize);

    if (header_buf[0] == 'W' && header_buf[1] == 'F' && header_buf[2] == 'X')
        result = S_OK;

    return result;
}

int WfxRawReader::ReadWfxAndExtra_()
{
    assert(wave_extra_.size() == sizeof WAVEFORMATEX);
    const UINT32 wfx_size = sizeof WAVEFORMATEX;
    ::memcpy(&wave_format_, &wave_extra_[0], wfx_size);
    wave_extra_.erase(wave_extra_.begin(), wave_extra_.begin() + wfx_size);
    assert(wave_extra_.empty());

    for (int i = 0; i < wave_format_.cbSize; ++i)
    {
        wave_extra_.push_back(static_cast<const char>(file_stream_.get()));
    }

    ++file_bytes_read_ += wave_format_.cbSize;
    return S_OK;
}

int WfxRawReader::Open(std::string file_name)
{
    if (file_name.length() < 1)
        return E_INVALIDARG;
    using std::ios;
    file_stream_.open(file_name.c_str(), ios::binary | ios::in);

    int result = -1;
    if (file_stream_.is_open())
    {
        // store file size
        using std::ifstream;
        ifstream::pos_type begin = file_stream_.tellg();
        file_stream_.seekg(0, ios::end);
        file_size_  = file_stream_.tellg() - begin;
        file_stream_.seekg(0, ios::beg);

        const UINT32 num_bytes_to_read = kWfxRawHeaderSize +
                                         sizeof WAVEFORMATEX;
        for (int i = 0; i < num_bytes_to_read; ++i)
        {
            wave_extra_.push_back(
                static_cast<const char>(file_stream_.get()));
            ++file_bytes_read_;
        }

        result = CheckHeader_();

        if (result == S_OK)
            result = ReadWfxAndExtra_();
    }

    return result;
}

int WfxRawReader::Close()
{
    file_stream_.close();
    return 0;
}

int WfxRawReader::Read(char_vector* ptr_out_data)
{
    if (!ptr_out_data)
        return E_INVALIDARG;

    assert(file_stream_.is_open());

    UINT32 old_out_size = (*ptr_out_data).size();

    UINT32 bytes_to_read = file_size_ - file_bytes_read_;

    for (int i = 0; i < bytes_to_read; ++i)
    {
        (*ptr_out_data).push_back(static_cast<const char>(file_stream_.get()));
        ++file_bytes_read_;
    }

    assert(old_out_size + bytes_to_read == (*ptr_out_data).size());

    return S_OK;
}

int WfxRawReader::GetWaveFormat(WAVEFORMATEX* ptr_out_wfx,
                                char_vector* ptr_out_extra)
{
    if (!ptr_out_wfx)
        return E_INVALIDARG;

    int result = E_FAIL;

    if (result == S_OK && ptr_out_extra)
    {
        std::copy(wave_extra_.begin(), wave_extra_.end(),
                  (*ptr_out_extra).begin());
        assert(wave_extra_.size() == (*ptr_out_extra).size());
        if (wave_extra_.size() == (*ptr_out_extra).size())
            result = S_OK;
    }
    return result;
}

} // WebmMfVorbisDecLib
