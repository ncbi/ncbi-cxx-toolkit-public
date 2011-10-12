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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *     Usage example for CAgpReader (CAgpErr, CAgpRow).
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_util.hpp>

USING_NCBI_SCOPE;

// Count objects, scaffolds, components, gaps
class CAgpCounter : public CAgpReader
{
public:
    int objects1, objects2, scaffolds, components, gaps, comments;
    int singleton_objects, singleton_scaffolds;
    int components_in_object, components_in_scaffold;
    CAgpCounter( EAgpVersion version_arg ) :
        CAgpReader( version_arg )
    {
        objects1=objects2=scaffolds=components=gaps=comments=0;
        singleton_objects=singleton_scaffolds=0;
        components_in_object=components_in_scaffold=0;
    }

#define P(x) cout << #x << "=" << x << "\n"
#define PPP(x, y, z) P(x); P(y); P(z)
    void PrintResults()
    {
        PPP(objects1, objects2, scaffolds);
        P(singleton_objects);
        P(singleton_scaffolds);
        cout << "\n";
        PPP(components, gaps, comments);
    }

    // Callbacks
    virtual void OnScaffoldEnd()
    {
        scaffolds++;
        if(components_in_scaffold==1) singleton_scaffolds++;
        components_in_scaffold=0;
    }

    virtual void OnObjectChange()
    {
        // If CAgpReader works properly, both counts are the same.
        if(!m_at_end) objects1++;
        if(!m_at_beg) {
          objects2++;

          if(components_in_object==1) singleton_objects++;
          components_in_object=0;
        }
    }

    virtual void OnGapOrComponent()
    {
        if(m_this_row->IsGap()) gaps++;
        else {
          components++;
          components_in_object++;
          components_in_scaffold++;
        }
    }

    virtual void OnComment()
    {
        comments++;
    }


};

int main(int argc, char* argv[])
{
    EAgpVersion agp_version = eAgpVersion_1_1;
    // rudimentary argument parsing, too simple to justify
    // heavyweight classes like CArgDescriptions
    for( int ii = 1; ii < argc; ++ii ) {
        const char *arg = argv[ii];
        if( string("-v2.0") == arg ) {
            agp_version = eAgpVersion_2_0;
        } else {
            cerr << "unknown arg: " << arg << endl;
            return 1;
        }
    }

    CAgpCounter reader( agp_version );
    int code=reader.ReadStream(cin);
    if(code) {
        // cerr << "Code " << code << "\n";
        string msg = reader.GetErrorMessage();
        cerr << msg;
        return 1;
    }
    else {
        reader.PrintResults(); // object/scaffold/gap/component counts
        return 0;
    }
}

