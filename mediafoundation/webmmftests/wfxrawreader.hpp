/*
 *  wfxrawreader.h
 *  sdlaudioplayer
 *
 *  Created by Tom Finegan on 10/24/10.
 *  Copyright 2010 WebM Project. All rights reserved.
 *
 */

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&); \
    void operator=(const TypeName&)
#endif

namespace WebmMfVorbisDecLib
{

#ifndef _WIN32
// alignment/packing going to be a problem?
typedef struct {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t  nBlockAlign;
    uint16_t  wBitsPerSample;
    uint16_t  cbSize;
} WAVEFORMATEX;
#endif


class WfxRawReader
{
public:
    typedef std::vector<char> char_vector;
    WfxRawReader();
    ~WfxRawReader();
    int Open(std::string file);
    int Close();
    // reads the all remaining file data into ptr_out_data
    int Read(std::vector<char>* ptr_out_data);
    int GetWaveFormat(WAVEFORMATEX* ptr_out_wave,
                      char_vector* ptr_out_extra_data);
private:
    int CheckHeader_();
    int ReadWfxAndExtra_();
    WAVEFORMATEX wave_format_;
    char_vector wave_extra_;
    //char_vector samples_;
    std::ifstream file_stream_;
    UINT32 file_size_;
    UINT32 file_bytes_read_;
    DISALLOW_COPY_AND_ASSIGN(WfxRawReader);
};

} // WebmMfVorbisDecLib
