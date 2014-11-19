#include <objbase.h>
#include "mkvparserstreamreader.hpp"

namespace mkvparser
{

IStreamReader::IStreamReader()
{
}

IStreamReader::~IStreamReader()
{
}

HRESULT IStreamReader::LockPages(const BlockEntry*)
{
    return S_OK;
}

void IStreamReader::UnlockPages(const BlockEntry*)
{
}

}  //end namespace mkvparser
