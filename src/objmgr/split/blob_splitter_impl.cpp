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

    // copying skeleton while stripping annotations
    CopySkeleton(*m_Skeleton, entry);

    // collect annot pieces separating annotations with different priorities
    CollectPieces();

    if ( m_Pieces.size() <= eAnnotPriority_skeleton ) {
        return false;
    }
    
    // split pieces in chunks
    SplitPieces();

    if ( m_Chunks.size() <= 1 ) { // only main chunk exists
        return false;
    }

    MakeID2SObjects();

    return m_SplitBlob.IsSplit();
}


void CBlobSplitterImpl::CollectPieces(void)
{
    // Collect annotation pieces and strip skeleton annotations
    // to main chunk.
    m_Pieces.clear();

    ITERATE ( TEntries, it, m_Entries ) {
        CollectPieces(it->second);
    }

    if ( m_Params.m_Verbose ) {
        // display pieces statistics
        CSize single_ref;
        ITERATE ( TPieces, pit, m_Pieces ) {
            if ( !*pit ) {
                continue;
            }
            ITERATE ( CAnnotPieces, it, **pit ) {
                if ( it->second.size() <= 1 ) {
                    single_ref += it->second.m_Size;
                }
                else {
                    NcbiCout << "@" << it->first.AsString() << ": " <<
                        it->second.m_Size << '\n';
                }
            }
        }
        if ( single_ref ) {
            NcbiCout << "with 1 obj: " << single_ref << '\n';
        }
        NcbiCout << NcbiEndl;
    }
}


void CBlobSplitterImpl::CollectPieces(const CPlace_SplitInfo& info)
{
    TPlaceId place_id = info.m_PlaceId;
    if ( info.m_Descr ) {
        Add(SAnnotPiece(place_id, *info.m_Descr));
    }
    ITERATE ( CPlace_SplitInfo::TSeq_annots, it, info.m_Annots ) {
        CollectPieces(place_id, it->second);
    }
    if ( info.m_Inst ) {
        const CSeq_inst_SplitInfo& inst_info = *info.m_Inst;
        ITERATE( CSeq_inst_SplitInfo::TSeq_data, it, inst_info.m_Seq_data ){
            Add(SAnnotPiece(place_id, *it));
        }
    }
    ITERATE ( CPlace_SplitInfo::TBioseqs, it, info.m_Bioseqs ) {
        Add(SAnnotPiece(place_id, *it));
    }
}


void CBlobSplitterImpl::CollectPieces(TPlaceId place_id,
                                      const CSeq_annot_SplitInfo& info)
{
    size_t max_size = info.m_Name.IsNamed()? 100: 10;
    size_t size = 0;
    ITERATE ( CSeq_annot_SplitInfo::TObjects, i, info.m_Objects ) {
        if ( *i ) {
            size += (*i)->size();
        }
    }
    bool add_as_whole = size <= max_size;
    if ( add_as_whole ) {
        // add whole Seq-annot as one piece because header overhead is too big
        Add(SAnnotPiece(place_id, info));
    }
    else {
        // add each annotation as separate piece
        ITERATE ( CSeq_annot_SplitInfo::TObjects, i, info.m_Objects ) {
            if ( !*i ) {
                continue;
            }
            ITERATE ( CLocObjects_SplitInfo, j, **i ) {
                Add(SAnnotPiece(place_id, info, *j));
            }
        }
    }
}


void CBlobSplitterImpl::Add(const SAnnotPiece& piece)
{
    EAnnotPriority priority = piece.m_Priority;
    m_Pieces.resize(max(m_Pieces.size(), priority + size_t(1)));
    if ( !m_Pieces[priority] ) {
        m_Pieces[priority] = new CAnnotPieces;
    }
    m_Pieces[priority]->Add(piece);
}


SChunkInfo* CBlobSplitterImpl::NextChunk(void)
{
    int chunk_id = m_Chunks.size();
    if ( m_Chunks.find(0) == m_Chunks.end() )
        ++chunk_id;
    return &m_Chunks[chunk_id];
}


SChunkInfo* CBlobSplitterImpl::NextChunk(SChunkInfo* chunk, const CSize& size)
{
    if ( chunk ) {
        CSize::TDataSize cur_size = chunk->m_Size.GetZipSize();
        CSize::TDataSize new_size = cur_size + size.GetZipSize();
        if ( cur_size < m_Params.m_MinChunkSize ||
             cur_size <= m_Params.m_ChunkSize &&
             new_size <= m_Params.m_MaxChunkSize ) {
            return chunk;
        }
    }
    return NextChunk();
}


void CBlobSplitterImpl::SplitPieces(void)
{
    NON_CONST_ITERATE ( TPieces, prit, m_Pieces ) {
        if ( !*prit ) {
            continue;
        }
        EAnnotPriority priority = EAnnotPriority(prit-m_Pieces.begin());
        if ( priority == eAnnotPriority_skeleton ) {
            AddToSkeleton(**prit);
        }
        else {
            SplitPieces(**prit);
        }
        _ASSERT((*prit)->empty());
        prit->Reset();
    }
    
    m_Pieces.clear();

    if ( m_Params.m_Verbose ) {
        // display collected chunks stats
        ITERATE ( TChunks, it, m_Chunks ) {
            NcbiCout << "Chunk: " << it->first << ": " << it->second.m_Size <<
                NcbiEndl;
        }
    }

    SChunkInfo& main_chunk = m_Chunks[0];
    if ( m_Params.m_JoinSmallChunks &&
         main_chunk.m_Size.GetZipSize() < m_Params.m_MinChunkSize ) {
        // attach too small chunks to the skeleton
        typedef multimap<double, int> TSizes;
        TSizes sizes;
        ITERATE ( TChunks, it, m_Chunks ) {
            if ( it->first != 0 ) {
                sizes.insert(TSizes::value_type(it->second.m_Size.GetZipSize(),
                                                it->first));
            }
        }
        
        if ( m_Params.m_Verbose ) {
            LOG_POST("Joining small chunks");
        }
        ITERATE ( TSizes, it, sizes ) {
            double new_size = main_chunk.m_Size.GetZipSize() + it->first;
            if ( new_size > m_Params.m_MinChunkSize ) {
                break;
            }
            main_chunk.Add(m_Chunks[it->second]);
            m_Chunks.erase(it->second);
        }
    }
}


void CBlobSplitterImpl::AddToSkeleton(CAnnotPieces& pieces)
{
    SChunkInfo& main_chunk = m_Chunks[0];

    // combine ids with small amount of pieces
    while ( !pieces.empty() ) {
        CAnnotPieces::iterator max_iter = pieces.begin();
        SIdAnnotPieces& objs = max_iter->second;
        if ( !objs.empty() ) {
            while ( !objs.empty() ) {
                SAnnotPiece piece = *objs.begin();
                main_chunk.Add(piece);
                pieces.Remove(piece);
                _ASSERT(objs.empty() || *objs.begin() != piece);
            }
        }
        _ASSERT(max_iter->second.empty());
        pieces.erase(max_iter);
    }
    _ASSERT(pieces.empty());
}


void CBlobSplitterImpl::SplitPieces(CAnnotPieces& pieces)
{
    SChunkInfo* chunk = 0;
    SChunkInfo* long_chunk = 0;
    
    // split ids with large amount of pieces
    while ( !pieces.empty() ) {
        // find id with most size of pieces on it
        CSize max_size;
        CAnnotPieces::iterator max_iter;
        NON_CONST_ITERATE ( CAnnotPieces, it, pieces ) {
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
        bool sequential = true;
        TRange prevRange = TRange::GetEmpty();
        ITERATE ( SIdAnnotPieces, it, objs ) {
            const SAnnotPiece& piece = *it;
            TRange range = piece.m_IdRange;
            if ( range.Empty() ) {
                continue;
            }
            if ( !prevRange.Empty() ) {
                if ( range.GetFrom() < prevRange.GetFrom() ||
                     (range.IntersectingWith(prevRange) &&
                      range != prevRange) ) {
                    sequential = false;
                    break;
                }
            }
            prevRange = range;
        }
        if ( !sequential ) {
            // extract long annotations first

            // calculate maximum piece length
            // how many chunks to make from these annotations
            size_t chunk_count =
                size_t(double(objs.m_Size.GetZipSize())/m_Params.m_ChunkSize
                       +.5);
            // length of sequence covered by annotations
            size_t whole_length = objs.m_IdRange.GetLength();
            // estimated length of sequence covered by one chunk
            size_t chunk_length = whole_length / chunk_count;
            // maximum length of one piece over the sequence
            size_t max_piece_length = chunk_length / 2;

            // extract long pieces into main or next chunk
            vector<SAnnotPiece> pcs;
            CSize size;
            ITERATE ( SIdAnnotPieces, it, objs ) {
                const SAnnotPiece& piece = *it;
                if ( piece.m_IdRange.GetLength() > max_piece_length ) {
                    pcs.push_back(piece);
                    size += piece.m_Size;
                    if ( m_Params.m_Verbose ) {
                        LOG_POST(" long piece: "<<piece.m_IdRange.GetLength());
                    }
                }
            }
            if ( !pcs.empty() ) {
                if ( m_Params.m_Verbose ) {
                    LOG_POST("  "<<pcs.size()<<" long pieces: "<<size);
                    LOG_POST("  "
                             " CC:"<<chunk_count<<
                             " WL:"<<whole_length<<
                             " CL:"<<chunk_length<<
                             " ML:"<<max_piece_length);
                }
                ITERATE ( vector<SAnnotPiece>, it, pcs ) {
                    const SAnnotPiece& piece = *it;
                    long_chunk = NextChunk(long_chunk, piece.m_Size);
                    long_chunk->Add(piece);
                    pieces.Remove(piece);
                }
            }
        }

        // extract all other pieces
        vector<SAnnotPiece> pcs;
        ITERATE ( SIdAnnotPieces, it, objs ) {
            pcs.push_back(*it);
        }
        ITERATE ( vector<SAnnotPiece>, it, pcs ) {
            const SAnnotPiece piece = *it;
            chunk = NextChunk(chunk, piece.m_Size);
            chunk->Add(piece);
            pieces.Remove(piece);
        }
        
        _ASSERT(max_iter->second.empty());
        pieces.erase(max_iter);
    }
    
    // combine ids with small amount of pieces
    while ( !pieces.empty() ) {
        CAnnotPieces::iterator max_iter = pieces.begin();
        SIdAnnotPieces& objs = max_iter->second;
        if ( !objs.empty() ) {
            chunk = NextChunk(chunk, objs.m_Size);
            while ( !objs.empty() ) {
                SAnnotPiece piece = *objs.begin();
                chunk->Add(piece);
                pieces.Remove(piece);
                _ASSERT(objs.empty() || *objs.begin() != piece);
            }
        }
        _ASSERT(max_iter->second.empty());
        pieces.erase(max_iter);
    }
    _ASSERT(pieces.empty());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2004/08/19 14:18:54  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.11  2004/08/04 14:48:21  vasilche
* Added joining of very small chunks with skeleton.
*
* Revision 1.10  2004/06/30 23:27:59  ucko
* Make sure to call max on identically-typed arguments.
*
* Revision 1.9  2004/06/30 20:56:32  vasilche
* Added splitting of Seqdesr objects (disabled yet).
*
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
