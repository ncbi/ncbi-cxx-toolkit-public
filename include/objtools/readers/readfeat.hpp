/*
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
 * Author:  Jonathan Kans
 *
 * File Description:
 *   Feature table reader
 *
 */

#ifndef OBJTOOLS_READERS___READFEAT__HPP
#define OBJTOOLS_READERS___READFEAT__HPP

#include <corelib/ncbistd.hpp>

// Objects includes
#include <objects/seq/Seq_annot.hpp>

#include <memory> // for auto_ptr<>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CFeature_table_reader_imp;

// public interface for (single instance) feature table reader

class NCBI_XOBJREAD_EXPORT CFeature_table_reader
{
public:
    // read 5-column feature table and return Seq-annot
    static CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIstream& ifs);
    static CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIstream& ifs,
                                                    const string& seqid,
                                                    const string& annotname);

private:
    // this class uses a singleton internally to manage the specifics
    // of the feature table reader implementation
    // these are the variables / functions that control the singleton
    static auto_ptr<CFeature_table_reader_imp> sm_Implementation;

    static void                       x_InitImplementation(void);
    static CFeature_table_reader_imp& x_GetImplementation (void);
};

inline
CFeature_table_reader_imp& CFeature_table_reader::x_GetImplementation(void)
{
    if ( !sm_Implementation.get() ) {
        x_InitImplementation();
    }
    return *sm_Implementation;
}


END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READFEAT__HPP
