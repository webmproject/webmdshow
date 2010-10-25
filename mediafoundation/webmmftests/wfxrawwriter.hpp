#ifndef __MEDIAFOUNDATION_WEBMMFVORBISDEC_WFXRAWWRITER_HPP__
#define __MEDIAFOUNDATION_WEBMMFVORBISDEC_WFXRAWWRITER_HPP__

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif


#ifdef _DEBUG
#include <fstream>
#include <ios>
#include <string>

namespace WebmMfVorbisDecLib
{

// Writes a file in the following format:
// ------------------------------------------------------
// | WAVEFORMATEX | WAVEFORMATEX.cbSize bytes | samples |
// ------------------------------------------------------
class WfxRawWriter
{
public:
    WfxRawWriter();
    ~WfxRawWriter();
    int Open(const WAVEFORMATEX* ptr_wave_format);
    int Write(const BYTE* const ptr_byte_buffer, UINT32 byte_count);
    int Close();
private:
    int Reopen_();
    bool closed_;
    WAVEFORMATEX wave_format_;
    std::ofstream file_stream_;
    std::string file_name_;
    DISALLOW_COPY_AND_ASSIGN(WfxRawWriter);
};

}
#endif // _DEBUG

#endif // __MEDIAFOUNDATION_WEBMMFVORBISDEC_WFXRAWWRITER_HPP__
