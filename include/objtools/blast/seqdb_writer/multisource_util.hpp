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
* Author:  Kevin Bealer
*
* File Description:
*     Utility functions and classes for multisource app.
*/

#ifndef OBJTOOLS_BLAST_SEQDB_WRITER___MULTISOURCE_UTIL__HPP
#define OBJTOOLS_BLAST_SEQDB_WRITER___MULTISOURCE_UTIL__HPP

#include <corelib/ncbistd.hpp>

// Blast databases
#include <objects/blastdb/Blast_def_line.hpp>

// SeqDB interface to blast databases
#include <objtools/blast/seqdb_reader/seqdb.hpp>

BEGIN_NCBI_SCOPE

/// Get all keys for a defline.
NCBI_XOBJWRITE_EXPORT
void GetDeflineKeys(const objects::CBlast_def_line & defline,
                    vector<string>        & keys);

/// CMultisourceException
///
/// This exception class is thrown for errors occurring during
/// traceback.

class NCBI_XOBJWRITE_EXPORT CMultisourceException : public CException {
public:
    /// Errors are classified into several types.
    enum EErrCode {
        /// Argument validation failed.
        eArg,
        /// Failed to create the output file(s)/directory
        eOutputFileError
    };

    /// Get a message describing the exception.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eArg: return "eArgErr";
        default:   return CException::GetErrCodeString();
        }
    }

    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CMultisourceException, CException);
};

/// Gi List for database construction.
///
/// This GI list is built from the set of identifiers the user has
/// specified for inclusion in the resulting database.  By using a
/// SeqDB GI list, database filtering can be done using SeqDB's
/// internal processing machinery.

class NCBI_XOBJWRITE_EXPORT CInputGiList : public CSeqDBGiList {
public:
    /// Construct an empty GI list.
    CInputGiList(int capacity = 1024)
        : m_Last(ZERO_GI)
    {
        if (capacity > 0) {
            m_GisOids.reserve(capacity);
        }

        // An empty vector is always sorted, right?
        m_CurrentOrder = eGi;
    }

    /// Append a GI.
    ///
    /// This method adds a GI to the list.
    ///
    /// @param gi A sequence identifier.
    void AppendGi(TGi gi, int oid = -1)
    {
        if (m_CurrentOrder == eGi) {
            if (m_Last > gi) {
                m_CurrentOrder = eNone;
            } else if (m_Last == gi) {
                return;
            }
        }

        m_GisOids.push_back(SGiOid(gi, oid));
        m_Last = gi;
    }

    /// Append a Seq-id
    ///
    /// This method adds a Seq-id to the list.
    ///
    /// @param seqid A sequence identifier.
    void AppendSi(const string &si, int oid = -1)
    {
        // This could verify ordering, but since ordering for GIs is
        // common, and ordering for Seq-ids is rare, for now I'll just
        // assume that Seq-ids are out-of order.  This also fits the
        // basic practice of not making tiny optimizations in code
        // paths that are slow.

        m_CurrentOrder = eNone;
        string str_id = SeqDB_SimplifyAccession(si);
        if (str_id != "") m_SisOids.push_back(SSiOid(str_id, oid));
    }

private:
    TGi m_Last;
};


NCBI_XOBJWRITE_EXPORT
void ReadTextFile(CNcbiIstream   & f,
                  vector<string> & lines);

class NCBI_XOBJWRITE_EXPORT CSequenceReturn {
public:
    CSequenceReturn(CSeqDB & seqdb, const char * buffer)
        : m_SeqDB(seqdb), m_Buffer(buffer)
    {
    }

    ~CSequenceReturn()
    {
        m_SeqDB.RetSequence(& m_Buffer);
    }

private:
    CSequenceReturn & operator=(CSequenceReturn &);

    CSeqDB     & m_SeqDB;
    const char * m_Buffer;
};

/// Maps Seq-id key to bitset.
typedef map<string, int> TIdToBits;

/// Map from Seq-id string to set of leaf taxids.
typedef map<string, CBlast_def_line::TTaxIds> TIdToLeafs;

/// Map from linkout bit number to list of ids.
typedef map<int, vector<string> > TLinkoutMap;

NCBI_XOBJWRITE_EXPORT
void MapToLMBits(const TLinkoutMap & gilist, TIdToBits & gi2links);

NCBI_XOBJWRITE_EXPORT
bool CheckAccession(const string  & acc,
                    TGi           & gi,
                    CRef<objects::CSeq_id> & seqid,
                    bool          & specific);

NCBI_XOBJWRITE_EXPORT
void GetSeqIdKey(const objects::CSeq_id & id, string & key);

NCBI_XOBJWRITE_EXPORT
string AccessionToKey(const string & acc);

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_WRITER___MULTISOURCE_UTIL__HPP

