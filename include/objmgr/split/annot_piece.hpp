#ifndef NCBI_OBJMGR_SPLIT_ANNOT_PIECE__HPP
#define NCBI_OBJMGR_SPLIT_ANNOT_PIECE__HPP

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


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <memory>
#include <map>
#include <set>

#include <objmgr/split/id_range.hpp>
#include <objmgr/split/size.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

class CAnnotObject_SplitInfo;
class CLocObjects_SplitInfo;
class CSeq_annot_SplitInfo;
class CBioseq_SplitInfo;
class CSeq_data_SplitInfo;

struct SChunkInfo;


struct SAnnotPiece
{
    typedef CSeqsRange::TRange TRange;

    SAnnotPiece(void);
    SAnnotPiece(const SAnnotPiece& piece, const COneSeqRange& range);
    SAnnotPiece(const CAnnotObject_SplitInfo& obj,
                const CSeq_annot_SplitInfo& annot);
    explicit SAnnotPiece(const CSeq_annot_SplitInfo& annot);
    explicit SAnnotPiece(const CSeq_data_SplitInfo& data);

    // sort by location first, than by Seq-annot ptr, than by object ptr.
    bool operator<(const SAnnotPiece& piece) const;
    bool operator==(const SAnnotPiece& piece) const;
    bool operator!=(const SAnnotPiece& piece) const;

    CSize m_Size;
    CSeqsRange m_Location;
    TRange m_IdRange;
    enum PieceType {
        empty,
        annot_object,
        seq_annot,
        seq_data
    };
    PieceType m_Type;
    const CAnnotObject_SplitInfo* m_Annot_object;
    const CSeq_annot_SplitInfo* m_Seq_annot;
    const CSeq_data_SplitInfo* m_Seq_data;
};


struct SIdAnnotPieces
{
    typedef CSeqsRange::TRange TRange;
    typedef set<SAnnotPiece> TPieces;
    typedef TPieces::const_iterator const_iterator;
    typedef TPieces::iterator iterator;

    SIdAnnotPieces(void)
        {
        }

    void Add(const SAnnotPiece& piece);
    void Remove(const SAnnotPiece& piece);

    iterator Erase(iterator it);
    void clear(void)
        {
            m_Pieces.clear();
            m_Size.clear();
            m_IdRange = TRange::GetEmpty();
        }
    bool empty(void) const
        {
            return m_Pieces.empty();
        }
    size_t size(void) const
        {
            return m_Pieces.size();
        }

    const_iterator begin(void) const
        {
            return m_Pieces.begin();
        }
    const_iterator end(void) const
        {
            return m_Pieces.end();
        }
    iterator begin(void)
        {
            return m_Pieces.begin();
        }
    iterator end(void)
        {
            return m_Pieces.end();
        }

    TPieces m_Pieces;
    CSize m_Size;
    TRange m_IdRange;
};


class CAnnotPieces
{
public:
    typedef map<CSeq_id_Handle, SIdAnnotPieces> TPiecesById;
    typedef TPiecesById::const_iterator const_iterator;
    typedef TPiecesById::iterator iterator;

    CAnnotPieces(void);
    ~CAnnotPieces(void);

    void Add(const CBioseq_SplitInfo& bioseq_info, SChunkInfo& main_chunk);
    void Add(const CSeq_annot_SplitInfo& annot, SChunkInfo& main_chunk);

    void Add(const CLocObjects_SplitInfo& objs,
             const CSeq_annot_SplitInfo& annot);

    void Add(const SAnnotPiece& piece);
    void Remove(const SAnnotPiece& piece);

    void erase(iterator it)
        {
            m_PiecesById.erase(it);
        }
    void clear(void)
        {
            m_PiecesById.clear();
        }
    bool empty(void) const
        {
            return m_PiecesById.empty();
        }
    const_iterator begin(void) const
        {
            return m_PiecesById.begin();
        }
    const_iterator end(void) const
        {
            return m_PiecesById.end();
        }
    iterator begin(void)
        {
            return m_PiecesById.begin();
        }
    iterator end(void)
        {
            return m_PiecesById.end();
        }

    size_t CountAnnotObjects(void) const;

private:
    CAnnotPieces(const CAnnotPieces&);
    CAnnotPieces& operator=(const CAnnotPieces&);

    TPiecesById m_PiecesById;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/06/15 14:05:49  vasilche
* Added splitting of sequence.
*
* Revision 1.3  2004/01/07 17:36:18  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.2  2003/11/26 23:04:56  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:23  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_ANNOT_PIECE__HPP
