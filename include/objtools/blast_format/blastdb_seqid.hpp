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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_seqid.hpp
 *  Definition of an identifier for a sequence in a BLAST database
 */

#ifndef OBJTOOLS_BLAST_FORMAT___BLASTDB_ID__HPP
#define OBJTOOLS_BLAST_FORMAT___BLASTDB_ID__HPP

#include <objtools/readers/seqdb/seqdb.hpp>

/** @addtogroup BlastFormatting
 *
 * @{
 */

BEGIN_NCBI_SCOPE

/// Encapsulates identifier to retrieve data from a BLAST database
class CBlastDBSeqId : public CObject
{
public:
    /// Default value for an invalid entry
    static const int kInvalid = -1;

    /// Default constructor, creates an invalid object
    CBlastDBSeqId() : m_OID(kInvalid), m_EntryChoice(eNone) {}

    /// Constructor which takes a string as input, can be a GI, accession,
    /// NCBI Seq-id
    /// @param entry string to parse [in]
    CBlastDBSeqId(const string& entry) : m_OID(kInvalid), m_EntryChoice(eNone) 
    {
        try { 
            m_EntrySpecified.m_GI = NStr::StringToInt(entry); 
            m_EntryChoice = eGi;
            return; 
        } catch (...) {}
        m_EntrySpecified.m_SequenceId = new string(entry);
        m_EntryChoice = eSeqId;
    }

    /// Constructor which takes a PIG as input
    /// @param pig PIG [in]
    CBlastDBSeqId(int pig) : m_OID(kInvalid) {
        m_EntrySpecified.m_PIG = pig;
        m_EntryChoice = ePig;
    }

    /// Copy constructor
    /// @param rhs object to copy [in]
    CBlastDBSeqId(const CBlastDBSeqId& rhs) {
        do_copy(rhs);
    }

    /// Assignment operator
    /// @param rhs object to copy [in]
    CBlastDBSeqId& operator=(const CBlastDBSeqId& rhs) {
        do_copy(rhs);
        return *this;
    }

    /// Destructor
    ~CBlastDBSeqId() {
        if (IsStringId()) {
            delete m_EntrySpecified.m_SequenceId;
        }
    }

    /// Convert this object to a string
    string AsString() const {
        string retval;
        switch (m_EntryChoice) {
        case ePig:      retval = "PIG " + GetPig(); break;
        case eGi:       retval = "GI " + GetGi(); break;
        case eSeqId:    retval = "'" + GetStringId() + "'"; break;
        case eNone:
                        if (GetOID() != CBlastDBSeqId::kInvalid) {
                            retval = "OID " + NStr::IntToString(GetOID());
                        }
                        break;
        default:
            abort();
        }
        return retval;
    }

    /// Stream insertion operator
    /// @param out stream to write to [in|out]
    /// @param id object to write [in]
    friend ostream& operator<<(ostream& out, const CBlastDBSeqId& id) {
        out << id.AsString();
        return out;
    }

    /// Does this object contain a GI?
    bool IsGi() const { return m_EntryChoice == eGi; }
    /// Does this object contain a PIG?
    bool IsPig() const { return m_EntryChoice == ePig; }
    /// Does this object contain a string identifier?
    bool IsStringId() const { return m_EntryChoice == eSeqId; }
    /// Does this object contain an OID?
    bool IsOID() const { return m_EntryChoice == eNone || m_OID != kInvalid; }

    /// Retrieve this object's GI
    int GetGi() const { return m_EntrySpecified.m_GI; }
    /// Retrieve this object's PIG
    int GetPig() const { return m_EntrySpecified.m_PIG; }
    /// Retrieve this object's string identifier
    const string& GetStringId() const { return *m_EntrySpecified.m_SequenceId; }

    /// Set this object's OID
    /// @param oid OID to set [in]
    void SetOID(CSeqDB::TOID oid) { m_OID = oid; }
    /// Retrieve this object's OID
    CSeqDB::TOID GetOID() const { return m_OID; }

private:
    /// This object's OID
    CSeqDB::TOID m_OID;

    /// Enumeration to distinguish the types of entries stored by this object
    enum EEntryChoices {
        eNone,  ///< Invalid
        ePig,   ///< PIG
        eGi,    ///< GI
        eSeqId  ///< Sequence identifier as string
    };
    /// Choice of entry set, only valid if 
    EEntryChoices m_EntryChoice;

    /// Union to hold the memory of the data stored
    union {
        int m_PIG;              ///< Store a PIG
        int m_GI;               ///< Store a GI
        string* m_SequenceId;   ///< Store a sequence identifier as a string
    } m_EntrySpecified;

    /// Copies an object of this type
    /// @param rhs object to copy [in]
    void do_copy(const CBlastDBSeqId& rhs) {
        if (this != &rhs) {
            m_OID = rhs.m_OID;
            m_EntryChoice = rhs.m_EntryChoice;
            if (IsStringId()) {
                _ASSERT(rhs.m_EntrySpecified.m_SequenceId);
                m_EntrySpecified.m_SequenceId = 
                    new string(*rhs.m_EntrySpecified.m_SequenceId);
            } else {
                m_EntrySpecified = rhs.m_EntrySpecified;
            }
        }
    }
};


END_NCBI_SCOPE

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___BLASTDB_ID__HPP */

