#pragma once
#include "mkvparser.hpp"

namespace mkvparser
{
    class IStreamReader : public IMkvReader
    {
    private:
        IStreamReader(const IStreamReader&);
        IStreamReader& operator=(const IStreamReader&);

    protected:
        IStreamReader();
        virtual ~IStreamReader();

    public:
        virtual HRESULT LockPages(const BlockEntry*);
        virtual void UnlockPages(const BlockEntry*);

    };

}  //end namespace mkvparser
