#ifndef OGGPARSER_HPP
#define OGGPARSER_HPP

#include <list>

namespace oggparser
{

//const int E_ERROR = -1;
const int E_FILE_FORMAT_INVALID = -2;
const int E_BUFFER_NOT_FULL = -3;
const int E_END_OF_FILE = -4;
const int E_READ_ERROR = -5;

class IOggReader
{
public:
    //TODO: the semantics here are still in-work:
    virtual long Read(long long pos, long len, unsigned char* buf) = 0;
    //virtual long Length(long long* total /* , long long* available */ ) = 0;
protected:
    virtual ~IOggReader();
};


long ReadInt(
    IOggReader*,
    long long pos,
    long len,
    long long& val);

class OggPage
{
public:

    //4: capture_pattern = "OggS"  (for every page in file???)
    //1: version
    //1: header_type
    //8: granule_pos
    //4: bitstream_serial_number
    //4: page_seq_num
    //4: crc
    //1: page_segments
    //1: segment_table
    //n: segments

    enum HeaderFlags
    {
        fContinued = 0x01,
        fBOS = 0x02,
        fEOS = 0x04,
        fDone = 0x80
    };

    struct Descriptor
    {
        long long pos;
        long len;
    };

    typedef std::list<Descriptor> descriptors_t;

    static long GetLength(const descriptors_t&);
    static long Copy(
        const descriptors_t&,
        IOggReader*,
        unsigned char* buf);
    static long Match(const descriptors_t&, IOggReader*, const char*);

    unsigned char capture_pattern[4];
    unsigned char version;
    unsigned char header;
    long long granule_pos;
    unsigned long serial_num;
    unsigned long sequence_num;
    unsigned long crc;  //signed or unsigned?
    descriptors_t descriptors;

    long Read(IOggReader*, long long&);
};

//rfc5334.txt
//Ogg Media Types


class OggStream
{
    OggStream(const OggStream&);
    OggStream& operator=(const OggStream&);

public:

    IOggReader* const m_pReader;

    explicit OggStream(IOggReader*);
    ~OggStream();

    struct Packet
    {
        OggPage::descriptors_t descriptors;
        long long granule_pos;

        long GetLength() const;
        long Copy(IOggReader*, unsigned char* buf) const;
        long IsHeader(IOggReader*, const char*) const;
    };

    typedef std::list<Packet> packets_t;

    long Init(Packet& ident, Packet& comment, Packet& setup);
    long Reset();
    long GetPacket(Packet&);

private:

    unsigned long m_serial_num;
    unsigned long m_page_num;
    unsigned long m_page_base;
    long long m_pos;
    long long m_base;

    long GetPacket(Packet&, int);
    long ParsePacket(Packet&);
    long ParsePage();

    packets_t m_packets;

};


class VorbisIdent
{
public:
    unsigned long version;
    unsigned char channels;
    unsigned long sample_rate;
    long bitrate_maximum;
    long bitrate_nominal;
    long bitrate_minimum;
    unsigned short blocksize_0;
    unsigned short blocksize_1;
    unsigned char framing;

    long Read(IOggReader*, const OggStream::Packet&);
};


}  //end namespace oggparser

#endif  //OGGPARSER_HPP
