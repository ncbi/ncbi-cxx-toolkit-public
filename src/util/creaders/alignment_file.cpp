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
 * Authors:  Josh Cherry
 *
 * File Description:  C++ wrappers for alignment file reading
 *
 */

#include <ncbi_pch.hpp>
#include <util/creaders/alnread.h>
#include <util/creaders/alignment_file.hpp>

BEGIN_NCBI_SCOPE


static char * ALIGNMENT_CALLBACK s_ReadLine(void *user_data)
{
    CNcbiIstream *is = static_cast<CNcbiIstream *>(user_data);
    if (!*is) {
        return 0;
    }
    string s;
    getline(*is, s);
    return strdup(s.c_str());
}


static void ALIGNMENT_CALLBACK s_ReportError (TErrorInfoPtr err_ptr,
                                              void* /* user_data */)
{
    if (err_ptr->category != eAlnErr_Fatal) {
        // ignore non-fatal errors
        return;
    }
    string msg = "Error reading alignment file";
    if (err_ptr->line_num > -1) {
        msg += " at line " + NStr::IntToString(err_ptr->line_num);
    }
    if (err_ptr->message) {
        msg += ":  ";
        msg += err_ptr->message;
    }
    throw runtime_error(msg);
}


void CAlignmentFile::Read(CNcbiIstream& is, const CSequenceInfo& info,
                          CAlignment& result)
{

    // make a SSequenceInfo corresponding to our CSequenceInfo argument
    SSequenceInfo sinfo;
    sinfo.alphabet = const_cast<char *>(info.GetAlphabet().c_str());
    sinfo.beginning_gap = const_cast<char *>(info.GetBeginningGap().c_str());;
    sinfo.end_gap = const_cast<char *>(info.GetEndGap().c_str());;
    sinfo.middle_gap = const_cast<char *>(info.GetMiddleGap().c_str());
    sinfo.missing = const_cast<char *>(info.GetMissing().c_str());
    sinfo.match = const_cast<char *>(info.GetMatch().c_str());

    // read the alignment stream
    TAlignmentFilePtr afp;
    afp = ReadAlignmentFile(s_ReadLine, (void *) &is,
                            s_ReportError, 0, &sinfo);
    if (!afp) {
        throw runtime_error("Error reading alignment");
    }

    // build the CAlignment
    result.SetSeqs().resize(afp->num_sequences);
    result.SetIds().resize(afp->num_sequences);
    for (int i = 0;  i < afp->num_sequences;  ++i) {
        result.SetSeqs()[i] = NStr::ToUpper(afp->sequences[i]);
        result.SetIds()[i] = afp->ids[i];
    }
    result.SetOrganisms().resize(afp->num_organisms);
    for (int i = 0;  i < afp->num_organisms;  ++i) {
        if (afp->organisms[i]) {
            result.SetOrganisms()[i] = afp->organisms[i];
        } else {
            result.SetOrganisms()[i].erase();
        }
    }
    result.SetDeflines().resize(afp->num_deflines);
    for (int i = 0;  i < afp->num_deflines;  ++i) {
        if (afp->deflines[i]) {
            result.SetDeflines()[i] = afp->deflines[i];
        } else {
            result.SetDeflines()[i].erase();
        }
    }

    AlignmentFileFree(afp);
}


void CAlignmentFile::ReadClustalProtein(CNcbiIstream& is,
                                        CAlignment& result)
{
    // IUPAC alphabet
    string prot_alphabet = "ABCDEFGHIKLMNPQRSTUVWXYZabcdefghiklmnpqrstuvwxyz";
    CSequenceInfo info;

    info.SetAlphabet(prot_alphabet);
    info.SetAllGap("-");

    CAlignmentFile::Read(is, info, result);
}


void CAlignmentFile::ReadClustalNuc(CNcbiIstream& is,
                                    CAlignment& result)
{
    // Nucleotide alphabet: IUPAC plus 'x'
    string nuc_alphabet = "ABCDGHKMNRSTUVWXYabcdghkmnrstuvwxy";

    CSequenceInfo info;

    info.SetAlphabet(nuc_alphabet);
    info.SetAllGap("-");

    CAlignmentFile::Read(is, info, result);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.6  2004/05/17 21:07:45  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/02/19 12:48:40  dicuccio
 * Roll back to version 1.3
 *
 * Revision 1.3  2004/02/11 17:58:12  gorelenk
 * Added missed modificator ALIGNMENT_CALLBACK to definitions of s_ReadLine
 * and ALIGNMENT_CALLBACK.
 *
 * Revision 1.2  2004/02/10 02:58:09  ucko
 * erase() strings rather than clear()ing them for compatibility with G++ 2.95.
 *
 * Revision 1.1  2004/02/09 16:02:34  jcherry
 * Initial versionnnn
 *
 * ===========================================================================
 */

