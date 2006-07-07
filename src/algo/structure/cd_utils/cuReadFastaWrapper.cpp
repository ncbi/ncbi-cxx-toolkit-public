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
 * Authors:  Chris Lanczycki
 *
 * File Description:
 *           A general interface to using various possible mechanisms
 *           for reading MFasta-formatted input, with concrete implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>

#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objtools/readers/fasta.hpp>

#include <objects/cdd/Global_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/structure/cd_utils/cuReadFastaWrapper.hpp>
//#include "ReadFastaWrapper.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)


bool CBasicFastaWrapper::ReadAsSeqEntry(CNcbiIstream& iStream, CRef< CSeq_entry >& seqEntry) 
{
    bool result = ReadFile(iStream);
    if (result) {
        seqEntry->Assign(*m_seqEntry);
    }
    return result;

}

bool CBasicFastaWrapper::ReadFile(CNcbiIstream& iStream) 
{
    bool result = (iStream);

    if (!result) {
        m_error = "Read Error:  invalid stream.\n";
    } else {

        
        CNcbiOstrstream oss;
        oss << iStream.rdbuf();

        m_activeFastaString = oss.str();
        if (m_cacheRawFasta) m_rawFastaString = m_activeFastaString;

        //  temporarily turn off warning messages (in case of '.' in *.a2m files)
         EDiagSev originalDiagSev = SetDiagPostLevel(eDiag_Error);  

        iStream.seekg(0);  
		try{
            m_seqEntry = ReadFasta(iStream, m_readFastaFlags);
		} catch (...) {
            result = false;
            m_seqEntry.Reset();
        }

        if (m_seqEntry.Empty()) {
            result = false;
            m_error = "Read Error:  empty seq entry.\n";
        }
        SetDiagPostLevel(originalDiagSev);
    }

    return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/07/07 16:54:46  lanczyck
 * use a try/catch around ReadFasta; modify diagnostic level altering code
 *
 * Revision 1.1  2006/03/29 15:44:07  lanczyck
 * add files for fasta->cd converter; change Makefile accordingly
 *
 * ===========================================================================
 */
