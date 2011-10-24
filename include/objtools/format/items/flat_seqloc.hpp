#ifndef OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP
#define OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- location representation
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declarations
class CInt_fuzz;
class CSeq_id;
class CSeq_interval;
class CSeq_loc;
class CBioseqContext;


class NCBI_FORMAT_EXPORT CFlatGapLoc : public CSeq_loc
{
public:
    typedef TSeqPos TLength;

    CFlatGapLoc(TLength value) : m_Length(value), m_Fuzz(NULL) { SetNull(); }

    TLength GetLength(void) const { return m_Length; }
    void SetLength(const TLength& value) { m_Length = value; }

    const CInt_fuzz* GetFuzz() const { return m_Fuzz; }
    void SetFuzz(const CInt_fuzz* fuzz) { m_Fuzz = fuzz; }

private:
    TLength m_Length;
    const CInt_fuzz* m_Fuzz;
};


class NCBI_FORMAT_EXPORT CFlatSeqLoc : public CObject // derived from CObject to allow for caching
{
public:
    enum EType
    {
        eType_location,     // Seq-loc
        eType_assembly      // Genome assembly
    };
    typedef EType     TType;

    CFlatSeqLoc(const CSeq_loc& loc, CBioseqContext& ctx, 
        TType type = eType_location);

    const string&     GetString(void)    const { return m_String;    }
    
private:

    enum EHTML {
        eHTML_None = 0,
        eHTML_Yes
    };

    enum EForce {
        eForce_None = 0,
        eForce_ToRange
    };

    bool x_Add(const CSeq_loc& loc, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(const CSeq_interval& si, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(const CSeq_point& pnt, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type, bool show_comp);
    bool x_Add(TSeqPos pnt, const CInt_fuzz* fuzz, CNcbiOstrstream& oss,
        EHTML html, EForce force = eForce_None );
    void x_AddID(const CSeq_id& id, CNcbiOstrstream& oss,
        CBioseqContext& ctx, TType type);

    bool x_IsAccessionVersion( CSeq_id_Handle id );

    // data
    string     m_String;    // whole location, as a GB-style string
    
    typedef std::map<CSeq_id_Handle, CSeq_id_Handle> TToAccessionMap;
    // a map of Seq_id_Handle to Seq_id_Handle (accession) which is
    // guarded by mutex (mutex needed since it's static)
    class CGuardedToAccessionMap {
    public:
        void Insert( CSeq_id_Handle from, CSeq_id_Handle to );
        // It's an empty Seq_id_Handle if not found
        CSeq_id_Handle Get( CSeq_id_Handle query );

    private:
        // make sure to use the mutex anytime you read/write m_TheMap
        CFastMutex m_MutexForTheMap;
        TToAccessionMap m_TheMap;
    };
    static CGuardedToAccessionMap m_ToAccessionMap;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___FLAT_SEQLOC__HPP */
