#ifndef __NCBI_FLATFILE_FILESET_HPP__
#define __NCBI_FLATFILE_FILESET_HPP__
/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*
*
*/

#include <corelib/ncbistl.hpp>

#include <objtools/writers/multi_source_file.hpp>
#include <fstream>

BEGIN_NCBI_SCOPE;
BEGIN_NAMESPACE(objects);

// _Enum options not need to be sequential numbers
template<class _Stream, class _Enum, _Enum..._options>
class TFileSet
{
public:
    using enum_type    = _Enum;
    using stream_type  = _Stream;
    using base_stream  = std::ostream;

    static constexpr size_t enum_size = sizeof...(_options);
    static constexpr std::array<enum_type, enum_size> sm_enum_mapping = {{_options...}};

    using streams_array     = std::array<stream_type, enum_size>;
    using streams_ptr_array = std::array<base_stream*, enum_size>;

    TFileSet() = default;
    TFileSet(const streams_ptr_array& ptrs): m_streams_ptrs{ptrs} {}

    void Reset() {
        for (auto& p: m_streams)
            p.reset();
        for (auto& p: m_streams_ptrs)
            p = nullptr;

    }

    base_stream* operator[](enum_type _index) const
    {
        size_t index = get_enum_index(_index);
        return m_streams_ptrs[index];
    }

    void Set(enum_type _index, std::unique_ptr<base_stream> ostr)
    {
        size_t index = get_enum_index(_index);
        m_streams[index] = std::move(ostr);
        m_streams_ptrs[index] = x_cast(m_streams[index]);
    }

    void Set(enum_type _index, base_stream& ostr)
    {
        size_t index = get_enum_index(_index);
        m_streams[index].reset();
        m_streams_ptrs[index] = x_cast(ostr);
    }

    TFileSet Clone()
    { // just copy pointers to streams
        return {m_streams_ptrs};
    }

    static size_t constexpr get_enum_index(enum_type _index)
    {
        return std::distance(sm_enum_mapping.begin(), std::find(sm_enum_mapping.begin(), sm_enum_mapping.end(), _index));
    }
protected:
    static std::ostream* x_cast(std::ostream& _orig)
    {
        return &_orig;
    }
    template<typename _T>
    static std::ostream* x_cast(std::unique_ptr<_T>& _orig)
    {
        return _orig.get();
    }
    template<typename _T>
    static std::ostream* x_cast(std::shared_ptr<_T>& _orig)
    {
        return _orig.get();
    }

private:
    streams_array     m_streams = {}; // owned streams
    streams_ptr_array m_streams_ptrs = {}; // just pointers
};

template<class _Enum, _Enum..._options>
class TMultiSourceFileSet
{
public:
    using base_stream  = std::ostream;
    using _MyType      = TMultiSourceFileSet<_Enum, _options...>;
    using fileset_type = TFileSet<std::unique_ptr<base_stream>, _Enum, _options...>;
    using enum_type    = typename fileset_type::enum_type;
    using writers_type = std::array<CMultiSourceWriter, fileset_type::enum_size>;
    using filestream   = std::ofstream;

    TMultiSourceFileSet() = default;

    void SetUseMT(bool use_mt) {
        m_use_mt = use_mt;
    }

    void Open(enum_type _enum, const std::string& filename) {
        if (m_use_mt) {
            size_t index = fileset_type::get_enum_index(_enum);
            m_writers[index].Open(filename);
        } else {
            auto  new_stream = std::make_unique<filestream>(filename);
            new_stream->exceptions( ios::failbit | ios::badbit );
            m_simple_files.Set(_enum, std::move(new_stream));
        }
    }

    void Open(enum_type _enum, base_stream& ostr) {
        if (m_use_mt) {
            size_t index = fileset_type::get_enum_index(_enum);
            m_writers[index].Open(ostr);
        } else {
            m_simple_files.Set(_enum, ostr);
        }
    }

    // set maximum number of writers in the underlying structure
    void SetDepth(size_t depth) {
        for(auto& w: m_writers)
            w.SetMaxWriters(depth);
    }

    void Reset() {
        for (auto& w: m_writers)
            w.Close(); // this will block and wait until all writers leave
        m_simple_files.Reset();
    }

    void FlushAll() {
        for (auto& w: m_writers)
            w.Flush(); // this will block and wait until all writers leave
    }

    // Create a new set of streams for thread,
    // this can block and wait if the limit of writers is reached
    [[nodiscard]] fileset_type MakeNewFileset() {
        fileset_type result;
        if (m_use_mt) {
            for(size_t i=0; i<fileset_type::enum_size; ++i) {
                if (m_writers[i].IsOpen()) {
                    auto new_stream = m_writers[i].NewStreamPtr();
                    // so we don't fail silently if, e.g., the output disk gets full
                    new_stream->exceptions( ios::failbit | ios::badbit );
                    result.Set(fileset_type::sm_enum_mapping[i], std::move(new_stream));
                }
            }
        } else {
            result = m_simple_files.Clone();
        }
        return result;
    }

protected:
    bool          m_use_mt = false;
    writers_type  m_writers;
    fileset_type  m_simple_files;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif
