#ifndef OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP
#define OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP

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
* Author:  Mati Shomrat
*
* File Description:
*   User interface for generating flat file reports from ASN.1
*   
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
//#include <objects/seqset/Seq_entry.hpp>
//#include <objmgr/scope.hpp>

#include <objtools/format/flat_file_flags.hpp>
#include <objtools/format/context.hpp>
//#include <util/range.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStream;
class CFlatItemOStream;
class CSeq_submit;
class CSeq_entry;
class CSeq_loc;
class CSeq_entry_Handle;


class NCBI_FORMAT_EXPORT CFlatFileGenerator : public CObject
{
public:
    // types
    typedef CRange<TSeqPos> TRange;

    // constructor / destructor
    CFlatFileGenerator(CScope&      scope,
                       TFormat      format = eFormat_GenBank,
                       TMode        mode   = eMode_GBench,
                       TStyle       style  = eStyle_Normal,
                       TFilter      filter = fSkipProteins,
                       TFlags       flags  = 0);
    ~CFlatFileGenerator(void);

    // Supply an annotation selector to be used in feature gathering.
    SAnnotSelector& SetAnnotSelector(void);

    void Generate(const CSeq_submit& submit, CNcbiOstream& os);
    void Generate(const CSeq_loc& loc, CNcbiOstream& os);
    void Generate(const CSeq_entry_Handle& entry, CNcbiOstream& os);

    // NB: the item ostream should be allocated on the heap!
    void Generate(const CSeq_entry_Handle& entry, CFlatItemOStream& item_os);
    void Generate(const CSeq_submit& submit, CFlatItemOStream& item_os);
    void Generate(const CSeq_loc& loc, CFlatItemOStream& item_os);

    // deprecated!
    void Generate(const CSeq_entry& entry, CNcbiOstream& os);
    void Generate(const CSeq_entry& entry, CFlatItemOStream& item_os);

private:
    CRef<CFFContext>    m_Ctx;

    // forbidden
    CFlatFileGenerator(const CFlatFileGenerator&);
    CFlatFileGenerator& operator=(const CFlatFileGenerator&);
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/03/25 20:30:53  shomrat
* Use Handles
*
* Revision 1.5  2004/02/11 22:46:01  shomrat
* enumerations moved to flat_file_flags.hpp
*
* Revision 1.4  2004/02/11 16:40:33  shomrat
* added HideCDDFeats flag
*
* Revision 1.3  2004/01/15 16:57:08  dicuccio
* Added private unimplemented copy ctor to satisfy MSVC
*
* Revision 1.2  2004/01/14 15:52:40  shomrat
* Added GFF format
*
* Revision 1.1  2003/12/17 19:51:49  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_GENERATOR__HPP */
