#include "mkvreader.hpp"
#include <cassert>
//#include <mfapi.h>
//#include <mferror.h>

MkvReader::MkvReader(IMFByteStream* pStream) : m_pStream(pStream)
{
    const ULONG n = m_pStream->AddRef();
    n;

#ifdef _DEBUG
    DWORD dw;

    const HRESULT hr = m_pStream->GetCapabilities(&dw);
    assert(SUCCEEDED(hr));
    assert(dw & MFBYTESTREAM_IS_READABLE);
    assert(dw & MFBYTESTREAM_IS_SEEKABLE);
    //TODO: check whether local, etc
    //TODO: could also check this earlier, in byte stream handler
#endif
}


MkvReader::~MkvReader()
{
    const ULONG n = m_pStream->Release();
    n;
}


int MkvReader::Read(long long pos, long len, unsigned char* buf)
{
    if (pos < 0)
        return -1;

    if (len <= 0)
        return 0;

    QWORD curr;

    HRESULT hr = m_pStream->Seek(msoBegin, pos, 0, &curr);
    assert(SUCCEEDED(hr));
    assert(curr == QWORD(pos));

    ULONG cbRead;

    hr = m_pStream->Read(buf, len,  &cbRead);
    assert(SUCCEEDED(hr));
    assert(cbRead == ULONG(len));

    return 0;  //means all requested bytes were read
}


int MkvReader::Length(long long* total, long long* avail)
{
    if (total == 0)
        return -1;

    if (avail == 0)
        return -1;

    QWORD curr;

    HRESULT hr = m_pStream->GetCurrentPosition(&curr);

    if (FAILED(hr))
        return -1;

    *avail = curr;

    QWORD len;

    hr = m_pStream->GetLength(&len);

    if (FAILED(hr))
        return -1;

    *total = len;

    return 0;
}
