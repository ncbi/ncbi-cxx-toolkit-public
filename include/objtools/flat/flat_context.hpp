#ifndef OBJECTS_FLAT___FLAT_CONTEXT__HPP
#define OBJECTS_FLAT___FLAT_CONTEXT__HPP

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
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
*/

#include <objtools/flat/flat_loc.hpp>
#include <objtools/flat/flat_reference.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

// NB: If you create one of these yourself (not recommended), you
// should allocate it on the heap rather than on the stack, as some
// other types store long-lived CRefs.

class CFlatContext : public CObject
{
public:
    CFlatContext(void)
        : m_Formatter(0), m_AdjustCoords(false),
          m_SegmentNum(0), m_SegmentCount(0), m_GI(0), m_Length(0),
          m_Mol(CSeq_inst::eMol_not_set), m_Biomol(CMolInfo::eBiomol_unknown),
          m_IsProt(false), m_IsTPA(false), m_IsWGSMaster(false),
          m_IsRefSeq(false), m_IsRefSeqGenome(false)
        { }

    void SetFlags(const CSeq_entry& entry, bool do_parents);

    // GI -> accession, if possible
    const CSeq_id& GetPreferredSynonym(const CSeq_id& id) const;

    const char* GetUnits(bool abbrev = true) const;

    typedef CMolInfo::TBiomol             TBiomol;
    typedef CSeq_inst::TMol               TMol;
    typedef vector<CRef<CFlatReference> > TReferences;

    // accessors; real data is now private
    IFlatFormatter&    GetFormatter   (void) const { return *m_Formatter;     }
    const CSeq_loc&    GetLocation    (void) const { return *m_Location;      }
    bool               AdjustCoords   (void) const { return m_AdjustCoords;   }
    const CBioseq_Handle& GetHandle   (void) const { return m_Handle;         }
    bool               InSegSet       (void) const { return m_SegMaster;      }
    const CBioseq*     GetSegMaster   (void) const { return m_SegMaster;      }
    unsigned int       GetSegmentNum  (void) const { return m_SegmentNum;     }
    unsigned int       GetSegmentCount(void) const { return m_SegmentCount;   }
    const TReferences& GetReferences  (void) const { return m_References;     }
    int                GetGI          (void) const { return m_GI;             }
    const string&      GetAccession   (void) const { return m_Accession;      }
    const CSeq_id&     GetPrimaryID   (void) const { return *m_PrimaryID;     }
    TSeqPos            GetLength      (void) const { return m_Length;         }
    TMol               GetMol         (void) const { return m_Mol;            }
    TBiomol            GetBiomol      (void) const { return m_Biomol;         }
    bool               IsProt         (void) const { return m_IsProt;         }
    bool               IsTPA          (void) const { return m_IsTPA;          }
    bool               IsWGSMaster    (void) const { return m_IsWGSMaster;    }
    bool               IsRefSeq       (void) const { return m_IsRefSeq;       }
    bool               IsRefSeqGenome (void) const { return m_IsRefSeqGenome; }

private:
    IFlatFormatter*     m_Formatter;
    CConstRef<CSeq_loc> m_Location;
    bool                m_AdjustCoords;
    CBioseq_Handle      m_Handle;
    // everything below is technically redundant but useful to keep around
    CConstRef<CBioseq>  m_SegMaster;
    unsigned int        m_SegmentNum, m_SegmentCount;
    TReferences         m_References;
    int                 m_GI;
    string              m_Accession; // with version
    CConstRef<CSeq_id>  m_PrimaryID; // corresponds to above accn
    TSeqPos             m_Length;
    TMol                m_Mol;
    TBiomol             m_Biomol;
    bool                m_IsProt;
    bool                m_IsTPA;
    bool                m_IsWGSMaster;
    bool                m_IsRefSeq;
    bool                m_IsRefSeqGenome;
    CConstRef<CSeq_loc> m_CachedLoc;
    CConstRef<CFlatLoc> m_CachedFlatLoc;

    friend class IFlatFormatter; // for m_Cached*
    // these help with initialization
    friend class CFlatHead;
    friend class CFlatLoc;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2003/10/08 21:09:58  ucko
* For segmented sequences, find and note the "master" sequence rather
* than just setting a flag.
*
* Revision 1.3  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_CONTEXT__HPP */
