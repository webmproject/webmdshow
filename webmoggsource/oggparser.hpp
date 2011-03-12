#ifndef OGGPARSER_HPP
#define OGGPARSER_HPP

namespace oggparser
{

//const int E_ERROR = -1;
const int E_FILE_FORMAT_INVALID = -2;
//const int E_BUFFER_NOT_FULL = -3;

class IOggReader
{
public:
    virtual int Read(long long pos, long len, unsigned char* buf) = 0;
    //virtual int Length(long long* total, long long* available) = 0;
protected:
    virtual ~IOggReader();
};


}  //end namespace oggparser

#endif  //OGGPARSER_HPP
