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
 * Authors:  Aaron Ucko
 *
 * File Description:
 *   Test of low-level score matrix support
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/tables/raw_scoremat.h>

#include <ctype.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CSMTestApplication : public CNcbiApplication
{
private:
    void Init(void);
    int  Run(void);
};


void CSMTestApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "score matrix test program");

    arg_desc->AddDefaultKey("sm", "matrix", "name of score matrix to use",
                            CArgDescriptions::eString, "blosum62");
    arg_desc->SetConstraint
        ("sm", &(*new CArgAllow_Strings,
                 "blosum45", "blosum62", "blosum80", "pam30", "pam70"));

    arg_desc->AddFlag("dump", "dump whole matrix");

    arg_desc->AddOptionalKey
        ("aa1", "AA",
         "first amino acid (may be a symbol or an NCBIstdaa number)",
         CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("aa2", "AA", "second amino acid", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


inline static string s_FormatAA(int aa) {
    return isprint(aa) ? string(1, (char)aa) : NStr::IntToString(aa);
}

inline static int s_ParseAA(string aa) {
    return isdigit(aa[0]) ? NStr::StringToInt(aa) : aa[0];
}


static void s_Dump(const SNCBIPackedScoreMatrix& psm,
                   SNCBIFullScoreMatrix& fsm)
{
#if 1
    cout << "Packed:\n\n ";
    {{
        int l = strlen(psm.symbols);
        for (int i = 0;  i < l;  ++i) {
            cout << "  " << psm.symbols[i];
        }
        cout << '\n';
        for (int i = 0;  i < l;  ++i) {
            cout << psm.symbols[i];
            for (int j = 0;  j < l;  ++j) {
                cout << setw(3) << (int)psm.scores[i * l + j];
            }
            cout << '\n';
        }
    }}
#else
    cout << "Packed:\n\n  " << psm.symbols << "\n\n";
    {{
        int l = strlen(psm.symbols);
        for (int i = 0;  i < l;  ++i) {
            cout << psm.symbols[i] << ' ';
            for (int j = 0;  j < l;  ++j) {
                cout << char(psm.scores[i * l + j] + '0');
            }
            cout << '\n';
        }
    }}
#endif
    cout << endl;

    cout << "Unpacked:\n\n    ";
    for (int i = 0;  i < NCBI_FSM_DIM;  ++i) {
        if (fsm.s[i][i] != psm.defscore) {
            string s = s_FormatAA(i);
            cout << s[s.length() - 1];
        }
    }
    cout << '\n';
    for (int i = 0;  i < NCBI_FSM_DIM;  ++i) {
        if (fsm.s[i][i] != psm.defscore) {
            // The use of c_str() here is to work around a GCC 2.95 bug.
            cout << setw(3) << s_FormatAA(i).c_str() << ' ';
            for (int j = 0;  j < NCBI_FSM_DIM;  ++j) {
                if (fsm.s[j][j] != psm.defscore) {
                    cout << char(fsm.s[i][j] + '0');
                }
            }
            cout << '\n';
        }
    }
}


int CSMTestApplication::Run(void)
{
    CArgs                          args = GetArgs();
    string                         sm   = args["sm"].AsString();
    const SNCBIPackedScoreMatrix*  psm;
    auto_ptr<SNCBIFullScoreMatrix> fsm(new SNCBIFullScoreMatrix);

    if        (sm == "blosum45") {
        psm = &NCBISM_Blosum45;
    } else if (sm == "blosum62") {
        psm = &NCBISM_Blosum62;
    } else if (sm == "blosum80") {
        psm = &NCBISM_Blosum80;
    } else if (sm == "pam30") {
        psm = &NCBISM_Pam30;
    } else if (sm == "pam70") {
        psm = &NCBISM_Pam70;
    } else {
        psm = 0;
        _TROUBLE;
    }

    NCBISM_Unpack(psm, fsm.get());

    if (args["dump"]) {
        s_Dump(*psm, *fsm);
    }
    if (args["aa1"]  &&  args["aa2"]) {
        int aa1 = s_ParseAA(args["aa1"].AsString());
        int aa2 = s_ParseAA(args["aa2"].AsString());
        cout << "Packed:   " << (int)NCBISM_GetScore(psm, aa1, aa2) << endl;
        cout << "Unpacked: " << (int)fsm->s[aa1][aa2] << endl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSMTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2005/05/04 18:35:01  lavr
 * CSMTestApplication::Run(): Always init psm
 *
 * Revision 1.3  2004/05/17 21:09:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/08/22 01:32:36  ucko
 * Fix for GCC 2.95.
 *
 * Revision 1.1  2003/08/21 19:48:21  ucko
 * Add tables library (shared with C) for raw score matrices, etc.
 *
 * ===========================================================================
 */
