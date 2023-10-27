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

BEGIN_NCBI_SCOPE

template<typename _T, class _ParentDeleter>
class TDeleteWhenOwn
{
public:
    // no assignments from _ParentDeleter possible
    // all should be performed within TReferenceOrOwnership
    void operator()(_T* ptr) const
    {
        if (m_need_delete)
            m_parent(ptr);
    }
    _ParentDeleter m_parent;
    bool m_need_delete = true;
};

template<typename _T, class _Deleter = typename std::unique_ptr<_T>::deleter_type >
class TReferenceOrOwnership: public std::unique_ptr<_T, TDeleteWhenOwn<_T, _Deleter> >
{
public:
    using _MyBase = std::unique_ptr<_T, TDeleteWhenOwn<_T, _Deleter> >;
    using pointer      = typename _MyBase::pointer;
    using element_type = typename _MyBase::element_type;
    using deleter_type = _Deleter;

    TReferenceOrOwnership() = default;

    void reset(pointer _other = pointer())
    {
        _MyBase::reset(_other);
        _MyBase::get_deleter().m_need_delete = false;
    }

    template<typename _Up, typename _Ep>
    TReferenceOrOwnership& operator=(std::unique_ptr<_Up, _Ep>&& _o)
    {
        _MyBase::reset(_o.release());
        _MyBase::get_deleter().m_parent = std::forward<_Ep>(_o.get_deleter());
        _MyBase::get_deleter().m_need_delete = true;
        return *this;
    }
};

// _Enum options not need to be sequential numbers
template<class _Stream, class _Enum, _Enum..._options>
class TFileSet
{
public:
    static constexpr size_t enum_size = sizeof...(_options);

    using enum_type     = _Enum;
    using stream_type   = _Stream;
    using base_stream   = std::ostream;
    using TStream       = TReferenceOrOwnership<stream_type>;
    using streams_array = std::array<TStream, enum_size>;

    static constexpr std::array<enum_type, enum_size> sm_enum_mapping = {{_options...}};

    static size_t constexpr get_enum_index(enum_type _index)
    {
        return std::distance(sm_enum_mapping.begin(), std::find(sm_enum_mapping.begin(), sm_enum_mapping.end(), _index));
    }

    TFileSet() = default;
    TStream& operator[](enum_type _index)
    {
        size_t index = get_enum_index(_index);
        return m_streams[index];
    }

    void Reset()
    {
        for (auto& p: m_streams)
            p.reset();
    }

    TFileSet Clone()
    { // just copy pointers to streams, do not change ownership
        TFileSet retval;
        for (size_t i=0; i<enum_size; ++i)
            retval.m_streams[i].reset(m_streams[i].get());

        return retval;
    }
private:
    streams_array m_streams;
};

template<class _Enum, _Enum..._options>
class TMultiSourceFileSet
{
public:
    using base_stream  = std::ostream;
    using _MyType      = TMultiSourceFileSet<_Enum, _options...>;
    using fileset_type = TFileSet<base_stream, _Enum, _options...>;
    using enum_type    = typename fileset_type::enum_type;
    using writers_type = std::array<CMultiSourceWriter, fileset_type::enum_size>;
    using filestream   = std::ofstream;

    using iterator     = typename writers_type::iterator;

    iterator begin() { return m_writers.begin(); }
    iterator end()   { return m_writers.end(); }

    TMultiSourceFileSet() = default;
    TMultiSourceFileSet(bool use_mt): m_use_mt{use_mt} {}

    explicit TMultiSourceFileSet(bool use_mt, fileset_type& parent)
      : m_use_mt{use_mt}
    {
        for(size_t i=0; i<fileset_type::enum_size; ++i) {
            enum_type _enum = fileset_type::sm_enum_mapping[i];
            auto parent_stream = parent[_enum].get();
            if (parent_stream) {
                if (m_use_mt)
                    m_writers[i].Open(*parent_stream);
                else
                    m_simple_files[_enum].reset(parent_stream);
            }
        }
    }

    void SetUseMT(bool use_mt) {
        m_use_mt = use_mt;
    }

    void SetFilename(enum_type _enum, const std::string& filename) {
        if (m_use_mt) {
            size_t index = fileset_type::get_enum_index(_enum);
            m_writers[index].OpenDelayed(filename);
        } else {
            auto new_stream = std::make_unique<filestream>();
            new_stream->exceptions( ios::failbit | ios::badbit );
            new_stream->open(filename);
            m_simple_files[_enum] = std::move(new_stream);
        }
    }

    void Open(enum_type _enum, const std::string& filename) {
        if (m_use_mt) {
            size_t index = fileset_type::get_enum_index(_enum);
            m_writers[index].Open(filename);
        } else {
            auto new_stream = std::make_unique<filestream>();
            new_stream->exceptions( ios::failbit | ios::badbit );
            new_stream->open(filename);
            m_simple_files[_enum] = std::move(new_stream);
        }
    }

    void Open(enum_type _enum, base_stream& ostr) {
        if (m_use_mt) {
            size_t index = fileset_type::get_enum_index(_enum);
            m_writers[index].Open(ostr);
        } else {
            m_simple_files[_enum].reset(&ostr);
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
                    result[fileset_type::sm_enum_mapping[i]] = std::move(new_stream);
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


END_NCBI_SCOPE

#endif
