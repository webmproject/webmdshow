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

    unsigned char m_capture_pattern[4];
    unsigned char m_version;
    unsigned char m_header;
    long long m_granule_pos;
    unsigned long m_serial_num;
    unsigned long m_sequence_num;
    unsigned long m_crc;  //signed or unsigned?
    descriptors_t m_descriptors;

    long Read(IOggReader*, long long&);
};

//rfc5334.txt
//Ogg Media Types
//

enum CodecType
{
    kVorbis  //0x01vorbis
};

struct VorbisCodec
{
    //sampling rate
    //etc
    //ident hdr
    //comment hdr
    //setup hdr
};


class OggStream
{
    OggStream(const OggStream&);
    OggStream& operator=(const OggStream&);

private:

    IOggReader* const m_pReader;
    explicit OggStream(IOggReader*);

public:

    static long Create(IOggReader*, OggStream*&);

public:

    ~OggStream();

    //We should have a way to identify the "tracks" in this stream.

    struct Packet
    {
        OggPage::descriptors_t descriptors;
        long long granule_pos;
    };

    long Parse();
    long GetPacket(Packet&);  //need way to identify which track

private:

    long long m_pos;

    typedef std::list<Packet> packets_t;
    packets_t m_packets;

    long Init();
    long ReadPage(OggPage&);

};


}  //end namespace oggparser

#endif  //OGGPARSER_HPP
