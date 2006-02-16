/* $Id$
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/validator/validator.hpp>
#include <util/static_map.hpp>

#include "validatorp.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);


// *********************** CValidator implementation **********************


CValidator::CValidator(CObjectManager& objmgr) :
    m_ObjMgr(&objmgr),
    m_PrgCallback(0),
    m_UserData(0)
{
}


CValidator::~CValidator(void)
{
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_entry& se,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&se));
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.Validate(se, 0, scope) ) {
        errors.Reset();
    }
    return errors;
}

CConstRef<CValidError> CValidator::Validate
(const CSeq_entry_Handle& seh,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&*seh.GetCompleteSeq_entry()));
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.SetProgressCallback(m_PrgCallback, m_UserData);
    if ( !imp.Validate(seh, 0) ) {
        errors.Reset();
    }
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_submit& ss,
 CScope* scope,
 Uint4 options)
{
    CRef<CValidError> errors(new CValidError(&ss));
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.Validate(ss, scope);
    return errors;
}


CConstRef<CValidError> CValidator::Validate
(const CSeq_annot_Handle& sah,
 Uint4 options)
{
    CConstRef<CSeq_annot> sar = sah.GetCompleteSeq_annot();
    CRef<CValidError> errors(new CValidError(&*sar));
    CValidError_imp imp(*m_ObjMgr, &(*errors), options);
    imp.Validate(sah);
    return errors;
}


void CValidator::SetProgressCallback(TProgressCallback callback, void* user_data)
{
    m_PrgCallback = callback;
    m_UserData = user_data;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

 
/*
* ===========================================================================
*
* $Log$
* Revision 1.65  2006/02/16 21:56:53  rsmith
* Use new objects::valerr class to store validation errors.
*
* Revision 1.64  2006/01/25 19:16:05  rsmith
* Validate(Seq-entry-handle)
*
* Revision 1.63  2006/01/24 16:21:03  rsmith
* Validate Seq-annot handles not bare Seq-annots.
* Get Seq entry handle one time and use it more.
*
* Revision 1.62  2005/12/06 19:27:01  rsmith
* Expose validator error codes. Allow conversion between codes and descriptions.
*
* Revision 1.61  2005/09/19 15:58:00  rsmith
* + ImproperBondLocation to terse explanations.
*
* Revision 1.60  2005/09/14 14:17:19  rsmith
* add validation of Bond locations.
*
* Revision 1.59  2005/06/28 17:36:20  shomrat
* Include more information in the each error object
*
* Revision 1.58  2005/01/24 17:16:48  vasilche
* Safe boolean operators.
*
* Revision 1.57  2004/09/22 13:51:10  shomrat
* + SEQ_INST_BadHTGSeq
*
* Revision 1.56  2004/09/21 19:08:56  shomrat
* + SEQ_FEAT_MrnaTransFail
*
* Revision 1.55  2004/09/21 18:35:46  shomrat
* Added LocusTagProductMismatch
*
* Revision 1.54  2004/09/21 15:54:07  shomrat
* + SEQ_FEAT_CDSmRNAmismatch, SEQ_FEAT_UnnecessaryException
*
* Revision 1.53  2004/08/09 14:54:10  shomrat
* Added eErr_SEQ_INST_CompleteTitleProblem and eErr_SEQ_INST_CompleteCircleProblem
*
* Revision 1.52  2004/08/04 17:47:34  shomrat
* + SEQ_FEAT_TaxonDbxrefOnFeature
*
* Revision 1.51  2004/08/03 13:39:58  shomrat
* + GENERIC_MedlineEntryPub
*
* Revision 1.50  2004/07/29 17:08:20  shomrat
* + SEQ_DESCR_TransgenicProblem
*
* Revision 1.49  2004/07/29 16:07:58  shomrat
* Separated error message from offending object's description; Added error group
*
* Revision 1.48  2004/07/07 13:26:56  shomrat
* + SEQ_FEAT_NoNameForProtein
*
* Revision 1.47  2004/06/25 15:59:18  shomrat
* + eErr_SEQ_INST_MissingGaps
*
* Revision 1.46  2004/06/25 14:55:59  shomrat
* changes to CValidError and CValidErrorItem; +SEQ_FEAT_DuplicateTranslExcept,SEQ_FEAT_TranslExceptAndRnaEditing
*
* Revision 1.45  2004/06/17 17:03:53  shomrat
* Added CollidingPublications check
*
* Revision 1.44  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/03/25 18:31:41  shomrat
* + SEQ_FEAT_GenesInconsistent
*
* Revision 1.42  2004/03/19 14:47:56  shomrat
* + SEQ_FEAT_PartialsInconsistent
*
* Revision 1.41  2004/03/10 21:22:35  shomrat
* + SEQ_DESCR_UnwantedCompleteFlag
*
* Revision 1.40  2004/03/01 18:38:58  shomrat
* Added alternative start codon error
*
* Revision 1.39  2004/02/25 15:52:15  shomrat
* Added CollidingLocusTags error
*
* Revision 1.38  2004/01/16 20:08:35  shomrat
* Added LocusTagProblem error
*
* Revision 1.37  2003/12/17 19:13:58  shomrat
* Added SEQ_PKG_GraphPackagingProblem
*
* Revision 1.36  2003/12/16 17:34:23  shomrat
* Added SEQ_INST_SeqLocLength
*
* Revision 1.35  2003/12/16 16:17:43  shomrat
* Added SEQ_FEAT_BadConflictFlag and SEQ_FEAT_ConflictFlagSet
*
* Revision 1.34  2003/11/14 15:55:09  shomrat
* added SEQ_INST_TpaAssemblyProblem
*
* Revision 1.33  2003/11/12 20:30:24  shomrat
* added SEQ_FEAT_MultipleCdsOnMrna
*
* Revision 1.32  2003/10/27 14:54:11  shomrat
* added SEQ_FEAT_UTRdoesNotAbutCDS
*
* Revision 1.31  2003/10/27 14:14:41  shomrat
* added SEQ_INST_SeqLitGapLength0
*
* Revision 1.30  2003/10/20 16:07:07  shomrat
* Added SEQ_FEAT_OnlyGeneXrefs
*
* Revision 1.29  2003/10/13 18:45:23  shomrat
* Added SEQ_FEAT_BadTrnaAA
*
* Revision 1.28  2003/09/03 18:24:46  shomrat
* added SEQ_DESCR_RefGeneTrackingWithoutStatus
*
* Revision 1.27  2003/08/06 15:04:30  shomrat
* Added SEQ_INST_InternalNsInSeqLit
*
* Revision 1.26  2003/07/21 21:17:45  shomrat
* Added SEQ_FEAT_MissingLocation
*
* Revision 1.25  2003/06/02 16:06:43  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.24  2003/05/28 16:22:13  shomrat
* Added MissingCDSProduct error.
*
* Revision 1.23  2003/05/14 20:33:31  shomrat
* Using CRef instead of auto_ptr
*
* Revision 1.22  2003/05/05 15:32:16  shomrat
* Added SEQ_FEAT_MissingCDSproduct
*
* Revision 1.21  2003/04/29 14:56:08  shomrat
* Changes to SeqAlign related validation
*
* Revision 1.20  2003/04/15 14:55:37  shomrat
* Implemented a progress callback mechanism
*
* Revision 1.19  2003/04/07 14:55:17  shomrat
* Added SEQ_FEAT_DifferntIdTypesInSeqLoc
*
* Revision 1.18  2003/04/04 18:36:55  shomrat
* Added Internal_Exception error; Changed limits of min/max severity
*
* Revision 1.17  2003/03/31 14:41:07  shomrat
* $id: -> $id$
*
* Revision 1.16  2003/03/28 16:27:40  shomrat
* Added comments
*
* Revision 1.15  2003/03/21 21:10:26  shomrat
* Added SEQ_DESCR_UnnecessaryBioSourceFocus
*
* Revision 1.14  2003/03/21 16:20:33  shomrat
* Added error SEQ_INST_UnexpectedIdentifierChange
*
* Revision 1.13  2003/03/20 18:54:00  shomrat
* Added CValidator implementation
*
* Revision 1.12  2003/03/10 18:12:50  shomrat
* Record statistics information for each item
*
* Revision 1.11  2003/03/06 19:33:02  shomrat
* Bug fix and code cleanup in CVAlidError_CI
*
* Revision 1.10  2003/02/24 20:20:18  shomrat
* Pass the CValidError object to the implementation class instead of the internal TErrs vector
*
* Revision 1.9  2003/02/12 17:38:58  shomrat
* Added eErr_SEQ_DESCR_Obsolete
*
* Revision 1.8  2003/02/07 21:10:55  shomrat
* Added eErr_SEQ_INST_TerminalNs
*
* Revision 1.7  2003/01/24 21:17:19  shomrat
* Added missing verbose strings
*
* Revision 1.5  2003/01/07 20:00:57  shomrat
* Member function GetMessage() changed to GetMsg() due o conflict
*
* Revision 1.4  2002/12/26 16:34:43  shomrat
* Typo
*
* Revision 1.3  2002/12/24 16:51:41  shomrat
* Changes to include directives
*
* Revision 1.2  2002/12/23 20:19:22  shomrat
* Redundan character removed
*
*
* ===========================================================================
*/

