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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/blob_splitter_impl.hpp>

#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/split/blob_splitter.hpp>
#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/annot_piece.hpp>
#include <objmgr/split/asn_sizer.hpp>
#include <objmgr/split/chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

template<class C>
inline
C& NonConst(const C& c)
{
    return const_cast<C&>(c);
}


/////////////////////////////////////////////////////////////////////////////
// CBlobSplitter interface method to avoid recompilation of two files
/////////////////////////////////////////////////////////////////////////////

bool CBlobSplitter::Split(const CSeq_entry& entry)
{
    CBlobSplitterImpl impl(m_Params);
    if ( impl.Split(entry) ) {
        m_SplitBlob = impl.GetBlob();
    }
    else {
        m_SplitBlob.Reset(entry);
    }
    return m_SplitBlob.IsSplit();
}


/////////////////////////////////////////////////////////////////////////////
// CBlobSplitterImpl
/////////////////////////////////////////////////////////////////////////////


static CAsnSizer s_Sizer;

bool CBlobSplitterImpl::Split(const CSeq_entry& entry)
{
    Reset();

#if 0
    size_t before_count = CountAnnotObjects(entry);
    NcbiCout << "Total: before: " << before_count << NcbiEndl;
#endif

    // copying skeleton while stripping annotations
    CopySkeleton(*m_Skeleton, entry);

    // collect annot pieces stripping landmark annotations to main chunk
    CollectPieces();
    if ( m_Pieces->empty() ) {
        return false;
    }
    
    // split pieces in chunks
    SplitPieces();

    if ( m_Chunks.size() <= 1 ) { // only main chunk exists
        return false;
    }

    MakeID2SObjects();

#if 0
    size_t after_count = CountAnnotObjects(*m_Skeleton) +
        CountAnnotObjects(m_ID2_Chunks);
    NcbiCout << "Total: in chunks: " << after_count << NcbiEndl;
    _ASSERT(before_count == after_count);
#endif

    return m_SplitBlob.IsSplit();
}


void CBlobSplitterImpl::CollectPieces(void)
{
    // Collect annotation pieces and strip landmark and long annotations
    // to main chunk.
    m_Pieces.reset(new CAnnotPieces);
    SChunkInfo& main_chunk = m_Chunks[0];
    ITERATE ( TBioseqs, it, m_Bioseqs ) {
        m_Pieces->Add(it->second, main_chunk);
    }

    if ( m_Params.m_Verbose ) {
        // display pieces statistics
        CSize single_ref;
        ITERATE ( CAnnotPieces, it, *m_Pieces ) {
            if ( it->second.size() <= 1 ) {
                single_ref += it->second.m_Size;
            }
            else {
                NcbiCout << "@" << it->first.AsString() << ": " <<
                    it->second.m_Size << '\n';
            }
        }
        if ( single_ref ) {
            NcbiCout << "with 1 obj: " << single_ref << '\n';
        }
        NcbiCout << NcbiEndl;
    }
#if 0
    {{
        _ASSERT(m_Chunks.size() == 1);
        // count objects
        size_t count = CountAnnotObjects(*m_Skeleton) + 
            m_Chunks.begin()->second.CountAnnotObjects() +
            m_Pieces->CountAnnotObjects();
        NcbiCout << "Total: in pieces: " << count << NcbiEndl;
    }}
#endif
}


SChunkInfo* CBlobSplitterImpl::NextChunk(void)
{
    _ASSERT(!m_Chunks.empty());
    int chunk_id = m_Chunks.size();
    return &m_Chunks[chunk_id];
}


SChunkInfo* CBlobSplitterImpl::NextChunk(SChunkInfo* chunk, const CSize& size)
{
    if ( chunk ) {
        CSize::TDataSize cur_size = chunk->m_Size.GetZipSize();
        CSize::TDataSize new_size = cur_size + size.GetZipSize();
        if ( cur_size <= m_Params.m_ChunkSize &&
             new_size <= m_Params.m_MaxChunkSize ) {
            return chunk;
        }
    }
    return NextChunk();
}


void CBlobSplitterImpl::SplitPieces(void)
{
    SChunkInfo& main_chunk = m_Chunks[0];
    
    SChunkInfo* chunk = 0;
    
    // split ids with large amount of pieces
    while ( !m_Pieces->empty() ) {
#if 0
        {{
            size_t count = CountAnnotObjects(*m_Skeleton) +
                m_Pieces->CountAnnotObjects();
            ITERATE ( TChunks, it, m_Chunks ) {
                count += it->second.CountAnnotObjects();
            }
            NcbiCout << "Total count: " << count << '\n';
        }}
#endif

        // find id with most size of pieces on it
        CSize max_size;
        CAnnotPieces::iterator max_iter;
        NON_CONST_ITERATE ( CAnnotPieces, it, *m_Pieces ) {
            if ( it->second.m_Size > max_size ) {
                max_iter = it;
                max_size = it->second.m_Size;
            }
        }
        if ( max_size.GetZipSize() < m_Params.m_MaxChunkSize ||
             max_size.GetCount() <= 1 ) {
            break;
        }

        // split this id
        if ( m_Params.m_Verbose ) {
            LOG_POST("Splitting @"<<max_iter->first.AsString()<<
                     ": "<<max_size);
        }

        SIdAnnotPieces& objs = max_iter->second;
        size_t max_piece_length; // too long annotations
        {{
            // how many chunks to make from these annotations
            size_t chunk_count =
                size_t(double(objs.m_Size.GetZipSize())/m_Params.m_ChunkSize
                       +.5);
            // length of sequence covered by annotations
            size_t whole_length = objs.m_IdRange.GetLength();
            // estimated length of sequence covered by one chunk
            size_t chunk_length = whole_length / chunk_count;
            // maximum length of one piece over the sequence
            max_piece_length = chunk_length / 4;
        }}

        {{
            // extract long pieces into main or next chunk
            vector<SAnnotPiece> pieces;
            CSize size;
            ITERATE ( SIdAnnotPieces, it, objs ) {
                const SAnnotPiece& piece = *it;
                if ( piece.m_IdRange.GetLength() > max_piece_length ) {
                    pieces.push_back(piece);
                    size += piece.m_Size;
                }
            }
            if ( !pieces.empty() ) {
                if ( m_Params.m_Verbose ) {
                    LOG_POST("  "<<pieces.size()<<" long pieces");
                }
                SChunkInfo* long_chunk;
                if ( size.GetZipSize() < m_Params.m_ChunkSize/2 )
                    long_chunk = &main_chunk;
                else
                    long_chunk = 0;
                ITERATE ( vector<SAnnotPiece>, it, pieces ) {
                    const SAnnotPiece& piece = *it;
                    if ( long_chunk != &main_chunk )
                        long_chunk = NextChunk(long_chunk, piece.m_Size);
                    long_chunk->Add(piece);
                    m_Pieces->Remove(piece);
                }
            }
        }}

        {{
            // extract all other pieces
            vector<SAnnotPiece> pieces;
            ITERATE ( SIdAnnotPieces, it, objs ) {
                pieces.push_back(*it);
            }
            ITERATE ( vector<SAnnotPiece>, it, pieces ) {
                const SAnnotPiece piece = *it;
                chunk = NextChunk(chunk, piece.m_Size);
                chunk->Add(piece);
                m_Pieces->Remove(piece);
            }
            
            _ASSERT(max_iter->second.empty());
            m_Pieces->erase(max_iter);
        }}
    }
    
    // combine ids with small amount of pieces
    while ( !m_Pieces->empty() ) {
#if 0
        {{
            size_t count = CountAnnotObjects(*m_Skeleton) +
                m_Pieces->CountAnnotObjects();
            ITERATE ( TChunks, it, m_Chunks ) {
                count += it->second.CountAnnotObjects();
            }
            NcbiCout << "Total count: " << count << '\n';
        }}
#endif

        CAnnotPieces::iterator max_iter = m_Pieces->begin();
        SIdAnnotPieces& objs = max_iter->second;
        if ( !objs.empty() ) {
            chunk = NextChunk(chunk, objs.m_Size);
            while ( !objs.empty() ) {
                SAnnotPiece piece = *objs.begin();
                chunk->Add(piece);
                m_Pieces->Remove(piece);
                _ASSERT(objs.empty() || *objs.begin() != piece);
            }
        }
        _ASSERT(max_iter->second.empty());
        m_Pieces->erase(max_iter);
    }
    _ASSERT(m_Pieces->empty());
    m_Pieces.reset();

#if 0
    {{
        size_t count = CountAnnotObjects(*m_Skeleton);
        ITERATE ( TChunks, it, m_Chunks ) {
            count += it->second.CountAnnotObjects();
        }
        NcbiCout << "Total count: " << count << '\n';
    }}
#endif

    if ( m_Params.m_Verbose ) {
        // display collected chunks stats
        ITERATE ( TChunks, it, m_Chunks ) {
            NcbiCout << "Chunk: " << it->first << ": " << it->second.m_Size <<
                NcbiEndl;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/06/15 14:05:50  vasilche
* Added splitting of sequence.
*
* Revision 1.7  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2004/03/05 17:40:34  vasilche
* Added 'verbose' option to splitter parameters.
*
* Revision 1.5  2004/01/07 17:36:23  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.4  2003/12/03 19:30:44  kuznets
* Misprint fixed
*
* Revision 1.3  2003/11/26 23:04:57  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:01  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:25  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
