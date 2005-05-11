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
 * File Description:  Text editor launching for use in SWIG-generated wrappers
 *
 */

%inline %{

class CTextEditor
{
public:
    enum EBackground
    {
        eGuess,  // Guess based on editor and environment (e.g., DISPLAY)
        eAlways, // Always background (not so useful)
        eNever   // Never background (useful for actually editing the file)
    };
    static void EditFile(const string& fname,
                         EBackground bg = eGuess,
                         long line_num = -1);
    static void DisplayText(const string& text, long line_num = -1);
    static void EditText(const string& orig_text,
                         string& edited_text,
                         long line_num = -1);
};

inline void
CTextEditor::EditFile(const string& fname, EBackground bg, long line_num)
{
    CNcbiEnvironment env;

    string editor;
    bool do_background = false;

#ifdef NCBI_OS_MSWIN
    editor = "notepad";
    if (bg == eAlways || bg == eGuess) {
        do_background = true;
    }
#else
    if (env.Get("VISUAL") != "") {
        editor = env.Get("VISUAL");
    } else if (env.Get("EDITOR") != "") {
        editor = env.Get("EDITOR");
    } else {
        editor = "vi";
    }

    if (bg == eAlways || (bg == eGuess && env.Get("DISPLAY") != "")) {
        // Known backgroundable editors.  gvim backgrounds itself
        // (unfortunately for non-background editing) so it's not listed.
        if (editor == "emacs" || editor == "xemacs" || editor == "nedit") {
            do_background = true;
        }
    }
#endif

    CExec::EMode mode = do_background ? CExec::eDetach : CExec::eWait;
    if (line_num >= 0) {
        string line_num_str = "+" + NStr::IntToString(line_num);
        CExec::SpawnLP(mode, editor.c_str(), line_num_str.c_str(),
                       fname.c_str(), NULL);
    } else {
        CExec::SpawnLP(mode, editor.c_str(), fname.c_str(), NULL);
    }
}

inline void CTextEditor::DisplayText(const string& text, long line_num)
{
    string fname = CFile::GetTmpName();
    {{
        ofstream os(fname.c_str());
        os << text;
    }}
    EditFile(fname, eGuess, line_num);
    // this is in gui
    //CDeleteAtExit::Add(fname);
}

inline void CTextEditor::EditText(const string& orig_text,
                           string& edited_text,
                           long line_num)
{
    // Put text into temp file
    string fname = CFile::GetTmpName();
    {{
        ofstream os(fname.c_str());
        os << orig_text;
    }}

    // Do the editing
    EditFile(fname, eNever, line_num);

    // Read the (possibly modified) file
    {{
        ifstream is(fname.c_str());
        edited_text.erase();
        const int kBuffSize = 2048;
        char *buf = new char[kBuffSize];
        while (is) {
            is.read(buf, kBuffSize);
            edited_text.append(buf, is.gcount());
        }
        delete buf;
    }}

    // Remove the temp file
    CFile tmp_file(fname);
    tmp_file.Remove();
}

%}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
