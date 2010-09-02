#pragma once
#include "mkvparser.hpp"
#include <mfidl.h>

class MkvReader : public mkvparser::IMkvReader
{
    MkvReader(const MkvReader&);
    MkvReader& operator=(const MkvReader&);

public:

    explicit MkvReader(IMFByteStream*);
    virtual ~MkvReader();

    virtual int Read(long long position, long length, unsigned char* buffer);
    virtual int Length(long long* total, long long* available);

private:

    IMFByteStream* const m_pStream;

};

