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


const char CFastaIOWrapper::gt = '>';
const char CFastaIOWrapper::nl = '\n';

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
    bool result = (iStream.good());

    if (!result) {
        m_error = "Read Error:  invalid stream.\n";
//    } else if (m_useOldReader) {
//        result = Old_ReadFile(iStream);
    } else {

        CNcbiOstrstream oss;
        oss << iStream.rdbuf();
        iStream.seekg(0);  

        m_activeFastaString = oss.str();
        if (m_cacheRawFasta) m_rawFastaString = m_activeFastaString;

        //  temporarily turn off warning messages (in case of '.' in *.a2m files)
        EDiagSev originalDiagSev = SetDiagPostLevel(eDiag_Error);  

	    try{
            if (m_useOldReader) {
                m_seqEntry = ReadFasta(iStream, m_readFastaFlags);
            } else {
                CStreamLineReader lineReader(iStream);
                CFastaReader fastaReader(lineReader, m_readFastaFlags);
                //CCounterManager counterMgr(reader.SetIDGenerator(), NULL);
                m_seqEntry = fastaReader.ReadSet();
            }

            //  If there is only one sequence in the fasta, the Seq-entry returned is a Bioseq and not a Bioseq-set.
            //  In that case, change the Bioseq to a Bioseq-set so caller doesn't have to manage multiple Seq-entry choices.
            if (m_seqEntry->IsSeq() && m_useBioseqSet) {
                CRef<CSeq_entry> bioseqFromFasta(new CSeq_entry);
                bioseqFromFasta->Assign(*m_seqEntry);

                m_seqEntry->Select(CSeq_entry::e_Set);
                m_seqEntry->SetSet().SetSeq_set().push_back(bioseqFromFasta);
            }

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


unsigned int CFastaIOWrapper::GetNumRead() const
{
    return count(m_activeFastaString.begin(), m_activeFastaString.end(), gt);
}

string CFastaIOWrapper::GetSubstring(const string& s, unsigned int index, bool isDefline) const
{
    string result = "";
    int nFound = -1;
    SIZE_TYPE pos = 0, nextPos = 0;
    while (nextPos != NPOS && nFound < (int) index) {
        nextPos = s.find(gt, pos);
        if (nextPos != NPOS) {
            ++nFound;
            ++nextPos;  //  advance to next character in the string
            pos = nextPos;
        }
//        cout << "nFound = " << nFound << "; pos = " << pos << "; nextPos = " << nextPos << endl;
    }
    if (pos > 0) --pos;

    if (pos != NPOS && nFound == (int) index) {
        nextPos = s.find(nl, pos);
        if (nextPos != NPOS) {
            if (isDefline) {
                nextPos = nextPos - pos;  //  no +1 as I don't care about the new line
                result = s.substr(pos, nextPos);
            } else {
                pos = nextPos + 1;  //  skip the newline
                nextPos = s.find(gt, pos);
                if (nextPos != NPOS) {
                    nextPos = nextPos - pos - 1;  //  -1 as I don't care about the last new line itself
                }
                result = s.substr(pos, nextPos);
            }
        }
    }
    return result;
}

string CFastaIOWrapper::GetActiveDefline(unsigned int index) const
{
    return GetSubstring(m_activeFastaString, index, true);
}

string CFastaIOWrapper::GetActiveSequence(unsigned int index) const
{
    return GetSubstring(m_activeFastaString, index, false);
}

string CFastaIOWrapper::GetRawDefline(unsigned int index) const
{
    if (!m_cacheRawFasta) 
        return "";
    else
        return GetSubstring(m_rawFastaString, index, true);
}

string CFastaIOWrapper::GetRawSequence(unsigned int index) const
{
    if (!m_cacheRawFasta) 
        return "";
    else
        return GetSubstring(m_rawFastaString, index, false);
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2006/11/01 17:35:50  lanczyck
 * add methods to access raw/active fasta strings
 *
 * Revision 1.4  2006/10/12 15:08:48  lanczyck
 * deprecate use of old ReadFasta method in favor of CFastaReader class
 *
 * Revision 1.3  2006/09/07 17:35:24  lanczyck
 * fixes so can read in file w/ a single sequence
 *
 * Revision 1.2  2006/07/07 16:54:46  lanczyck
 * use a try/catch around ReadFasta; modify diagnostic level altering code
 *
 * Revision 1.1  2006/03/29 15:44:07  lanczyck
 * add files for fasta->cd converter; change Makefile accordingly
 *
 * ===========================================================================
 */
