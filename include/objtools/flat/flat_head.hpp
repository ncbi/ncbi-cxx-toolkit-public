#error don't include me
#ifndef OBJECTS_FLAT___FLAT_HEAD__HPP
#define OBJECTS_FLAT___FLAT_HEAD__HPP

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
*   New (early 2003) flat-file generator -- representation of "header"
*   data, which translates into a format-dependent sequence of paragraphs.
*
*/

#include <objtools/flat/flat_formatter.hpp>

#include <serial/enumvalues.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/EMBL_block.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// Ultimately split into several paragraphs in a format-dependent manner
// (e.g., LOCUS, DEFINITION, ACCESSION, [PID,] VERSION, DBSOURCE for GenPept)
class CFlatHead : public IFlatItem
{
public:
    // also fills in some fields of ctx, which we don't bother duplicating
    CFlatHead(CFlatContext& ctx);
    void Format(IFlatFormatter& f) const { f.FormatHead(*this); }

    typedef CEMBL_block::TDiv    TEMBLDiv;
    typedef CSeq_inst::TStrand   TStrand;
    typedef CSeq_inst::TTopology TTopology;

    const string&       GetLocus       (void) const { return m_Locus;         }
    TStrand             GetStrandedness(void) const { return m_Strandedness;  }
    const char*         GetMolString   (void) const;
    TTopology           GetTopology    (void) const { return m_Topology;      }
    string              GetDivision    (void) const;
    const CDate&        GetUpdateDate  (void) const { return *m_UpdateDate;   }
    const CDate&        GetCreateDate  (void) const { return *m_CreateDate;   }
    const string        GetDefinition  (void) const { return m_Definition;    }
    const CBioseq::TId& GetOtherIDs    (void) const { return m_OtherIDs;      }
    const list<string>& GetSecondaryIDs(void) const { return m_SecondaryIDs;  }
    const list<string>& GetDBSource    (void) const { return m_DBSource;      }
    const CSeqdesc*     GetProteinBlock(void) const { return m_ProteinBlock;  }

private:
    void x_AddDate(const CDate& date);

    void x_AddDBSource(void);
    // helpers for various components
    string x_FormatDBSourceID(const CSeq_id& id);
    void x_AddPIRBlock(void);
    void x_AddSPBlock(void);
    void x_AddPRFBlock(void);
    void x_AddPDBBlock(void);

    string              m_Locus;
    TStrand             m_Strandedness;
    TTopology           m_Topology;
    const string*       m_GBDivision;
    TEMBLDiv            m_EMBLDivision;
    CConstRef<CDate>    m_UpdateDate;
    CConstRef<CDate>    m_CreateDate;
    string              m_Definition;
    CBioseq::TId        m_OtherIDs; // current
    list<string>        m_SecondaryIDs; // predecessors
    list<string>        m_DBSource; // for proteins; relatively free-form
    CConstRef<CSeqdesc> m_ProteinBlock;

    CRef<CFlatContext>  m_Context;
};


inline
string CFlatHead::GetDivision(void) const
{
    if (m_Context->GetFormatter().GetDatabase() == IFlatFormatter::eDB_EMBL) {
        return CEMBL_block::GetTypeInfo_enum_EDiv()->FindName
            (m_EMBLDivision, true);
    } else if (m_GBDivision) {
        return *m_GBDivision;
    } else {
        return "UNK";
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/11/04 15:02:22  dicuccio
* Add explicit #error to catch compilation use
*
* Revision 1.4  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.3  2003/04/24 16:15:57  vasilche
* Added missing includes and forward class declarations.
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

#endif  /* OBJECTS_FLAT___FLAT_HEAD__HPP */
