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

//#include <memory>
//#include <ostream>

BEGIN_NCBI_SCOPE;
BEGIN_NAMESPACE(objects);

template<class _Stream, class _Enum, _Enum..._options>
class TFileSet
{
public:
    using enum_type = _Enum;
    using stream_type = _Stream;

    static constexpr size_t enum_size = sizeof...(_options);
    static constexpr std::array<enum_type, enum_size> sm_enum_mapping = {{_options...}};
    static constexpr size_t not_open = std::numeric_limits<size_t>::max();

    using streams_array = std::array<stream_type, enum_size>;
    using streams_ptr_array = std::array<std::ostream*, enum_size>;
    using redirects_type = std::array<size_t, enum_size>;

    TFileSet() = default;

    template<size_t _N>
    TFileSet(std::array<std::ostream*, _N>&& _init2)
    {
        for(size_t i=0; i<_N && i<enum_size; ++i) {
            m_streams_ptrs[i] = _init2[i];
        }
    }

    template<size_t _N, typename _T>
    TFileSet(std::array<_T, _N>&& _init)
    {
        for(size_t i=0; i<_N && i<enum_size; ++i) {
            m_streams[i] = std::move(_init[i]);
            m_streams_ptrs[i] = x_cast(m_streams[i]);
        }
    }

    template<size_t _N, typename _T>
    TFileSet(std::array<_T, _N>&& _init, const redirects_type& redirects)
    {
        for(size_t i=0; i<_N && i<enum_size; ++i) {
            m_streams[i] = std::move(_init[i]);
            size_t index = redirects[i];
            if (index == not_open)
                m_streams_ptrs[i] = nullptr;
            else
                m_streams_ptrs[i] = x_cast(m_streams[index]);
        }
    }

    std::ostream* GetStream(enum_type _index)
    {
        size_t index = get_enum_index(_index);
        return m_streams_ptrs[index];
    }

    template<typename _Other>
    TFileSet<_Other, _Enum, _options...> recast();

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
    streams_array     m_streams = {};
    streams_ptr_array m_streams_ptrs = {};
};

template<class _Enum, _Enum..._options>
class TMultiSourceFileSet
{
public:
    using _MyType = TMultiSourceFileSet<_Enum, _options...>;
    using fileset_type   = TFileSet<std::unique_ptr<std::ostream>, _Enum, _options...>;
    using enum_type      = typename fileset_type::enum_type;
    using writers_type   = std::array<CMultiSourceWriter, fileset_type::enum_size>;
    using filenames_type = std::array<std::string, fileset_type::enum_size>;

    TMultiSourceFileSet() {
        for(auto& rec: m_redirects)
            rec = fileset_type::not_open;
    }

    void SetFilename(enum_type _enum, const std::string& filename) {
        size_t index = fileset_type::get_enum_index(_enum);
        m_filenames[index] = filename;
        if (!filename.empty())
            m_redirects[index] = index;
    }

    void Redirect(enum_type _enum, enum_type _other) {
        size_t index1 = fileset_type::get_enum_index(_enum);
        size_t index2 = fileset_type::get_enum_index(_other);
        m_redirects[index1] = index2;
    }

    [[nodiscard]] fileset_type OverrideAll(std::ostream& ostr) {
        std::array<std::ostream*, fileset_type::enum_size> _init;
        for(auto& rec: _init)
            rec = &ostr;

        return {std::move(_init)};
    }

    [[nodiscard]] fileset_type OpenFilesSimple()
    {
        typename fileset_type::streams_array streams;
        for(size_t i=0; i<fileset_type::enum_size; ++i) {
            if (!m_filenames[i].empty()) {
                std::unique_ptr<std::ofstream> new_stream{new std::ofstream(m_filenames[i])};
                new_stream->exceptions( ios::failbit | ios::badbit );
                streams[i] = std::move(new_stream);
            }
        }
        return {std::move(streams), m_redirects};
    }

    void OpenFilesForMT() {
        for(size_t i=0; i<fileset_type::enum_size; ++i) {
            if (!m_filenames[i].empty()) {
                m_writers[i].Open(m_filenames[i]);
            }
        }
    }

    [[nodiscard]] fileset_type MakeNewStreams() {
        typename fileset_type::streams_array streams;
        for(size_t i=0; i<fileset_type::enum_size; ++i) {
            if (!m_filenames[i].empty()) {
                auto new_stream = m_writers[i].NewStream();
                // so we don't fail silently if, e.g., the output disk gets full
                new_stream.exceptions( ios::failbit | ios::badbit );
                std::unique_ptr<std::ostream> ostr = std::make_unique<CMultiSourceOStream>(std::move(new_stream));
                streams[i] = std::move(ostr);
            }
        }
        return {std::move(streams), m_redirects};
    }

    bool Has(enum_type _enum) const {
        size_t index = fileset_type::get_enum_index(_enum);
        return (!m_filenames[index].empty());
    }

protected:
    filenames_type m_filenames;
    writers_type   m_writers;
    typename fileset_type::redirects_type m_redirects;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif
