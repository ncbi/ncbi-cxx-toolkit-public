#if !defined(MZXML_READER_HPP)
#define MZXML_READER_HPP

/* ===========================================================================
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
 * $Id$
 * Author:  Douglas Slotta
 * File Description: The SAX parser of mzXML via xmlwrapp
 */

#include <string>
#include <memory>
#include <stack>

#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

USING_SCOPE(ncbi);

class MzXmlReader : public xml::event_parser {
    
public:
    
    MzXmlReader(CRef<CMsHdf5> msHdf5);
    ~MzXmlReader();

private:

    MzXmlReader(const MzXmlReader &);
    MzXmlReader & operator=(const MzXmlReader &);
    
    bool start_element(const string &name, const attrs_type &attrs);
    bool end_element(const string &name);
    bool text(const string &data);
    bool warning(const string &message);

    void convertPeaks(Uint4 peakCount, string &peaks, vector<float> &mz, vector<float> &it);    
    
    string m_elementText;
   
    CRef<CMsHdf5> m_msHdf5;
    stack<Uint4> m_scanStack;
    stack<Uint4> m_peaksCountStack;
    stack<string> m_peaksStack;
    stack<string> m_scanInfoStack;
    stack<Uint4> m_msLevelStack;
    bool m_inMsRun;
    string m_lastStartElement;
    string m_metadata;
    int m_parentScanOverride;
};

#endif
