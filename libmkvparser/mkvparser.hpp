// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "cmediatypes.hpp"
#include <vector>
#include <map>
#include <set>
#include <string>
#include <deque>
#include <list>
#include <functional>

namespace MkvParser
{

interface IMkvFile
{
    virtual HRESULT MkvRead(LONGLONG pos, LONG len, BYTE* pb) = 0;
    virtual HRESULT MkvLength(LONGLONG* pTotal, LONGLONG* pAvailable) = 0;
};

typedef std::vector<BYTE> bytes_t;

__int64 GetUIntLength(IMkvFile*, LONGLONG, long&);
__int64 ReadUInt(IMkvFile*, LONGLONG, long&);

__int64 SyncReadUInt(IMkvFile*, LONGLONG pos, LONGLONG stop, long&);

__int64 UnserializeUInt(IMkvFile*, LONGLONG pos, __int64 size);
float Unserialize4Float(IMkvFile*, LONGLONG);
double Unserialize8Double(IMkvFile*, LONGLONG);
signed char Unserialize1SInt(IMkvFile*, LONGLONG);
SHORT Unserialize2SInt(IMkvFile*, LONGLONG);
bool Match(IMkvFile*, LONGLONG&, ULONG, __int64&);
bool Match(IMkvFile*, LONGLONG&, ULONG, std::string&);
bool Match(IMkvFile*, LONGLONG&, ULONG, std::wstring&);
bool Match(IMkvFile*, LONGLONG&, ULONG, bytes_t&);
bool Match(IMkvFile*, LONGLONG&, ULONG, double&);
bool Match(IMkvFile*, LONGLONG&, ULONG, SHORT&);


struct EBMLHeader
{
    __int64 m_version;
    __int64 m_readVersion;
    __int64 m_maxIdLength;
    __int64 m_maxSizeLength;
    std::string m_docType;
    __int64 m_docTypeVersion;
    __int64 m_docTypeReadVersion;

    __int64 Parse(IMkvFile*, LONGLONG&);
};


class Segment;
class Track;
class Cluster;

class Block
{
    Block(const Block&);
    Block& operator=(const Block&);

public:
    const __int64 m_start;
    const __int64 m_size;

    Block(__int64 start, __int64 size, IMkvFile*);

    ULONG GetNumber() const;
    SHORT GetRelativeTimeCode() const;

    __int64 GetTimeCode(Cluster*) const;  //absolute, but not scaled
    __int64 GetTime(Cluster*) const;      //absolute, and scaled (ns units)
    //BYTE GetFlags() const;
    bool IsKey() const;
    void SetKey(bool);

    LONG GetSize() const;
    HRESULT Read(IMkvFile*, BYTE*) const;

private:
    __int64 m_track;   //Track::Number()
    SHORT m_timecode;  //relative to cluster
    BYTE m_flags;
    __int64 m_frame_off;
    LONG m_frame_size;

};


class BlockEntry
{
    BlockEntry(const BlockEntry&);
    BlockEntry& operator=(const BlockEntry&);

public:
    virtual ~BlockEntry();

    typedef std::deque<BlockEntry*> entries_t;
    typedef entries_t::size_type index_t;

    virtual bool EOS() const = 0;
    virtual Cluster* GetCluster() const = 0;
    virtual index_t GetIndex() const = 0;
    virtual const Block* GetBlock() const = 0;
    virtual bool IsBFrame() const = 0;

protected:
    BlockEntry();

};


class SimpleBlock : public BlockEntry
{
    SimpleBlock(const SimpleBlock&);
    SimpleBlock& operator=(const SimpleBlock&);

public:
    SimpleBlock(Cluster*, index_t, __int64 start, __int64 size);

    bool EOS() const;
    Cluster* GetCluster() const;
    index_t GetIndex() const;
    const Block* GetBlock() const;
    bool IsBFrame() const;

protected:
    Cluster* const m_pCluster;
    const index_t m_index;
    Block m_block;

};


class BlockGroup : public BlockEntry
{
    BlockGroup(const BlockGroup&);
    BlockGroup& operator=(const BlockGroup&);

public:
    BlockGroup(Cluster*, index_t, __int64, __int64);
    ~BlockGroup();

    bool EOS() const;
    Cluster* GetCluster() const;
    index_t GetIndex() const;
    const Block* GetBlock() const;
    bool IsBFrame() const;

    SHORT GetPrevTimeCode() const;  //relative to block's time
    SHORT GetNextTimeCode() const;  //as above

protected:
    Cluster* const m_pCluster;
    const index_t m_index;

private:
    BlockGroup(Cluster*, index_t, ULONG);
    void ParseBlock(__int64 start, __int64 size);

    SHORT m_prevTimeCode;
    SHORT m_nextTimeCode;

    //TODO: the Matroska spec says you can have multiple blocks within the
    //same block group, with blocks ranked by priority (the flag bits).
    //For now we just cache a single block.
#if 0
    typedef std::deque<Block*> blocks_t;
    blocks_t m_blocks;  //In practice should contain only a single element.
#else
    Block* m_pBlock;
#endif

};


class Track
{
    Track(const Track&);
    Track& operator=(const Track&);

public:
    Segment* const m_pSegment;
    virtual ~Track();

    BYTE GetType() const;
    ULONG GetNumber() const;
    const wchar_t* GetName() const;
    const wchar_t* GetCodecName() const;
    const char* GetCodecId() const;
    const bytes_t& GetCodecPrivate() const;

    const BlockEntry* GetEOS() const;

    struct Settings
    {
        __int64 start;
        __int64 size;
    };

    struct Info
    {
        __int64 type;
        __int64 number;
        __int64 uid;
        std::wstring name;
        std::string codecId;
        bytes_t codecPrivate;
        std::wstring codecName;
        Settings settings;
    };

    HRESULT GetFirst(const BlockEntry*&) const;

    HRESULT GetNextBlock(
        const BlockEntry* pCurr,
        const BlockEntry*& pNext) const;

    virtual HRESULT GetNextTime(
                const BlockEntry* pCurr,
                const BlockEntry* pNextBlock,
                const BlockEntry*& pNextTime) const = 0;

    virtual bool VetEntry(const BlockEntry*) const = 0;

protected:
    Track(Segment*, const Info&);
    const Info m_info;

    class EOSBlock : public BlockEntry
    {
    public:
        EOSBlock();

        bool EOS() const;
        Cluster* GetCluster() const;
        index_t GetIndex() const;
        const Block* GetBlock() const;
        bool IsBFrame() const;
    };

    EOSBlock m_eos;

};


class VideoTrack : public Track
{
    VideoTrack(const VideoTrack&);
    VideoTrack& operator=(const VideoTrack&);

public:
    VideoTrack(Segment*, const Info&);
    __int64 GetWidth() const;
    __int64 GetHeight() const;
    double GetFrameRate() const;

    HRESULT GetNextTime(
        const BlockEntry* pCurr,
        const BlockEntry* pNextBlock,
        const BlockEntry*& pNextTime) const;

    bool VetEntry(const BlockEntry*) const;

private:
    __int64 m_width;
    __int64 m_height;
    double m_rate;

};


class AudioTrack : public Track
{
    AudioTrack(const AudioTrack&);
    AudioTrack& operator=(const AudioTrack&);

public:
    AudioTrack(Segment*, const Info&);
    double GetSamplingRate() const;
    __int64 GetChannels() const;
    __int64 GetBitDepth() const;

    HRESULT GetNextTime(
        const BlockEntry* pCurr,
        const BlockEntry* pNextBlock,
        const BlockEntry*& pNextTime) const;

    bool VetEntry(const BlockEntry*) const;

private:
    double m_rate;
    __int64 m_channels;
    __int64 m_bit_depth;

};


class Tracks
{
    Tracks(const Tracks&);
    Tracks& operator=(const Tracks&);

public:
    Segment* const m_pSegment;
    const __int64 m_start;
    const __int64 m_size;

    Tracks(Segment*, __int64 start, __int64 size);
    virtual ~Tracks();

    Track* GetTrack(ULONG) const;

private:
    typedef std::map<ULONG, Track*> tracks_map_t;
    tracks_map_t m_tracks_map;

    struct Less
    {
        bool operator()(const Track* lhs, const Track* rhs) const
        {
            return (lhs->GetNumber() < rhs->GetNumber());
        }
    };

    typedef std::set<Track*, Less> tracks_set_t;
    tracks_set_t m_video_tracks_set;
    tracks_set_t m_audio_tracks_set;

    void ParseTrackEntry(__int64, __int64);

    template<typename E, typename T>
    void EnumerateTracksSet(const E& e, const tracks_set_t& tt) const
    {
        typedef tracks_set_t::const_iterator iter_t;

        iter_t i = tt.begin();
        const iter_t j = tt.end();

        while (i != j)  //TODO: replace with STL equivalent
        {
            T* const pTrack = static_cast<T*>(*i++);
            e(pTrack);
        }
    }

public:
    template<typename E>
    void EnumerateVideoTracks(const E& e) const
    {
        EnumerateTracksSet<E, VideoTrack>(e, m_video_tracks_set);
    }

    template<typename E>
    void EnumerateAudioTracks(const E& e) const
    {
        EnumerateTracksSet<E, AudioTrack>(e, m_audio_tracks_set);
    }

    template<typename E>
    void EnumerateTracks(const E& e) const
    {
        typedef tracks_map_t::const_iterator iter_t;

        iter_t i = m_tracks_map.begin();
        const iter_t j = m_tracks_map.end();

        while (i != j)
        {
            const tracks_map_t::value_type& value = *i++;
            const Track* const pTrack = value.second;
            e(pTrack);
        }
    }

};


class SegmentInfo
{
    SegmentInfo(const SegmentInfo&);
    SegmentInfo& operator=(const SegmentInfo&);

public:
    Segment* const m_pSegment;
    const __int64 m_start;
    const __int64 m_size;

    SegmentInfo(Segment*, __int64 start, __int64 size);
    __int64 GetTimeCodeScale() const;
    __int64 GetDuration() const;  //scaled
    const wchar_t* GetMuxingApp() const;
    const wchar_t* GetWritingApp() const;

private:
    __int64 m_timecodeScale;
    double m_duration;
    std::wstring m_muxingApp;
    std::wstring m_writingApp;

};


class CuePoint
{
public:
    void Parse(IMkvFile*, __int64 start, __int64 size);

    __int64 m_timecode;               //absolute but unscaled
    __int64 GetTime(Segment*) const;  //absolute and scaled (ns units)

    struct TrackPosition
    {
        __int64 m_track;
        __int64 m_pos;  //cluster
        __int64 m_block;
        //codec_state  //defaults to 0
        //reference = clusters containing req'd referenced blocks
        //  reftime = timecode of the referenced block
    };

    typedef std::list<TrackPosition> track_positions_t;
    track_positions_t m_track_positions;

    const TrackPosition* Find(const Track*) const;

    class CompareTime : std::binary_function<__int64, CuePoint, bool>
    {
        CompareTime& operator=(const CompareTime&);
    public:
        Segment* const m_pSegment;

        explicit CompareTime(Segment* p) : m_pSegment(p) {}
        CompareTime(const CompareTime& rhs) : m_pSegment(rhs.m_pSegment) {}

        __int64 GetTime(const CuePoint& cp) const
        {
            return cp.GetTime(m_pSegment);
        }

        bool operator()(__int64 left_ns, const CuePoint& cp) const
        {
            return (left_ns < GetTime(cp));
        }

        bool operator()(const CuePoint& cp, __int64 right_ns) const
        {
            return (GetTime(cp) < right_ns);
        }

        bool operator()(const CuePoint& lhs, const CuePoint& rhs) const
        {
            return (lhs.m_timecode < rhs.m_timecode);
        }
    };

private:
    void ParseTrackPosition(IMkvFile*, __int64, __int64);

};


class Cues
{
    Cues(const Cues&);
    Cues& operator=(const Cues&);

public:
    Segment* const m_pSegment;
    const __int64 m_start;
    const __int64 m_size;

    Cues(Segment*, __int64 start, __int64 size);

    bool Find(  //lower bound of time_ns
        __int64 time_ns,
        const Track*,
        const CuePoint*&,
        const CuePoint::TrackPosition*&) const;

    bool FindNext(  //upper_bound of time_ns
        __int64 time_ns,
        const Track*,
        const CuePoint*&,
        const CuePoint::TrackPosition*&) const;

private:
    typedef std::deque<CuePoint> cue_points_t;
    cue_points_t m_cue_points;

};



class Cluster
{
    Cluster(const Cluster&);
    Cluster& operator=(const Cluster&);

public:
    typedef std::deque<Cluster*> clusters_t;
    typedef clusters_t::size_type index_t;

    Segment* const m_pSegment;
    const index_t m_index;

public:
    static Cluster* Parse(Segment*, index_t, __int64 off);

    Cluster();  //EndOfStream
    ~Cluster();

    bool EOS() const;

    __int64 GetSize() const;  //total size of cluster

    //TimeCode (unscaled) and Time (ns) of the cluster itself:
    __int64 GetTimeCode();  //absolute, but not scaled
    __int64 GetTime();      //absolute, and scaled (nanosecond units)

    //Time (ns) of first (earliest) block:
    __int64 GetFirstTime();

    const BlockEntry* GetFirst();
    const BlockEntry* GetLast();
    const BlockEntry* GetNext(const BlockEntry*) const;

    const BlockEntry* GetEntry(const Track*);
    const BlockEntry* GetEntry(
        const CuePoint&,
        const CuePoint::TrackPosition&);

    const BlockEntry* GetMaxKey(const VideoTrack*);

    struct CompareTime : std::binary_function<__int64, Cluster*, bool>
    {
        bool operator()(__int64 left_ns, Cluster* c) const
        {
            return (left_ns < c->GetTime());
        }

        bool operator()(Cluster* c, __int64 right_ns) const
        {
            return (c->GetTime() < right_ns);
        }

        bool operator()(Cluster* lhs, Cluster* rhs) const
        {
            return lhs->GetTimeCode() < rhs->GetTimeCode();
        }
    };

    struct ComparePos : std::binary_function<__int64, Cluster*, bool>
    {
        bool operator()(__int64 left_pos, Cluster* c) const
        {
            return (left_pos < _abs64(c->m_pos));
        }

        bool operator()(Cluster* c, __int64 right_pos) const
        {
            return (_abs64(c->m_pos) < right_pos);
        }

        bool operator()(Cluster* lhs, Cluster* rhs) const
        {
            return (_abs64(lhs->m_pos) < _abs64(rhs->m_pos));
        }
    };

protected:
    Cluster(Segment*, index_t, __int64 off);

public:
    __int64 m_pos;
    __int64 m_size;  //size of payload

private:
    __int64 m_timecode;
    BlockEntry::entries_t m_entries;

    void Load();
    void LoadBlockEntries();
    void ParseBlockGroup(__int64, __int64);
    void ParseSimpleBlock(__int64, __int64);

};


class Segment
{
    Segment(const Segment&);
    Segment& operator=(const Segment&);

private:
    Segment(IMkvFile*, __int64 pos, __int64 size);

public:
    IMkvFile* const m_pFile;
    const __int64 m_start;  //posn of segment payload
    const __int64 m_size;   //size of segment payload
    Cluster m_eos;  //TODO: make private?

    //__int64 GetStop() const;

    static __int64 CreateInstance(IMkvFile*, LONGLONG, Segment*&);
    ~Segment();

    //for big-bang loading (source filter)
    HRESULT Load();

    //for incremental loading (splitter)
    __int64 Unparsed() const;
    __int64 ParseHeaders();
    HRESULT ParseCluster(Cluster*&, __int64& newpos) const;
    bool AddCluster(Cluster*, __int64);

    const Tracks* GetTracks() const;
    const SegmentInfo* GetInfo() const;
    __int64 GetDuration() const;

    //NOTE: this turned out to be too inefficient.
    //__int64 Load(__int64 time_nanoseconds);

    Cluster* GetFirst();
    Cluster* GetLast();
    ULONG GetCount() const;

    Cluster* GetNext(const Cluster*);
    Cluster* GetPrevious(const Cluster*);

    Cluster* GetCluster(__int64 time_nanoseconds);

    void GetCluster(
        __int64 time_nanoseconds,
        Track*,
        Cluster*&,
        const BlockEntry*&);

    const Cues* GetCues() const;

private:
    __int64 m_pos;  //absolute file posn; what has been consumed so far
    //SeekHead* m_pSeekHead;
    SegmentInfo* m_pInfo;
    Tracks* m_pTracks;
    Cues* m_pCues;
    Cluster::clusters_t m_clusters;
    //Cluster::index_t m_index;
    void ParseSeekHead(__int64 pos, __int64 size);
    void ParseSeekEntry(__int64 pos, __int64 size);
    void ParseSecondarySeekHead(__int64 off);
    void ParseCues(__int64 off);

    bool SearchCues(
        __int64 time_ns,
        Track*,
        Cluster*&,
        const BlockEntry*&);

};


}  //end namespace MkvParser
