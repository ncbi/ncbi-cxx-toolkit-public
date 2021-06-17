#ifndef BAM_TEST_COMMON__H
#define BAM_TEST_COMMON__H

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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Common part for bam test applications
 *
 */

#include <corelib/ncbiapp.hpp>
#include <util/range.hpp>
#include <objtools/readers/idmapper.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CBAMTestCommon : public CNcbiApplication
{
public:
    void InitCommonArgs(CArgDescriptions& args, const char* bam_name = 0);
    bool ParseCommonArgs(const CArgs& args);

    struct SOStreamHolder {
        CNcbiOstream* out;
        
        template<class V>
        CNcbiOstream& operator<<(const V& v)
            {
                return *out << v;
            }
        CNcbiOstream& operator<<(CNcbiOstream& (*f)(CNcbiOstream&))
            {
                return *out << f;
            }
    };

    bool verbose;
    bool refseq_table;
    string path;
    string index_path;
    AutoPtr<IIdMapper> id_mapper;
    SOStreamHolder out;
    struct SQuery {
        string refseq_id;
        CRange<TSeqPos> refseq_range;
    };
    vector<SQuery> queries;
    bool by_start;
};

#endif // BAM_TEST_COMMON__H
