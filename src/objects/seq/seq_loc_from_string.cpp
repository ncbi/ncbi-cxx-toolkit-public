/* $Id$
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
 * Author:  Mati Shomrat, Michael Kornbluh
 *
 * File Description:
 *   Utilities for converting string to CSeq_loc.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include <objects/seq/seq_loc_from_string.hpp>

#include <objects/seq/seq_loc_reverse_complementer.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

#include <util/static_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// This anonymous namespace holds types and functions that are
// privately used by GetSeqLocFromString
namespace {

    class CLexToken 
    {
    public:
        CLexToken(unsigned int token_type) { m_TokenType = token_type; m_HasError = false; }
        virtual ~CLexToken() {}
        unsigned int GetTokenType() { return m_TokenType; }
        bool HasError () { return m_HasError; }

        virtual unsigned int GetInt() { return 0; }
        virtual string GetString() { return ""; }

        virtual CRef<CSeq_loc> GetLocation(CSeq_id *id, CGetSeqLocFromStringHelper* helper) { return CRef<CSeq_loc>(NULL); }
        enum E_TokenType {
            e_Int = 0,
            e_String,
            e_ParenPair,
            e_Join,
            e_Order,
            e_Complement,
            e_DotDot,
            e_LeftPartial,
            e_RightPartial,
            e_Comma,
            e_Accession
        };

    protected:
        unsigned int m_TokenType;
        bool m_HasError;
    };

    typedef vector<CLexToken *>  TLexTokenArray;

    bool s_ParseLex (string text, TLexTokenArray &token_list);

    class CLexTokenString : public CLexToken
    {
    public:
        CLexTokenString (string token_data);
        virtual ~CLexTokenString();
        virtual string GetString() { return m_TokenData; };
    private:
        string m_TokenData;
    };

    CLexTokenString::CLexTokenString(string token_data) : CLexToken (e_String)
    {
        m_TokenData = token_data;
    }

    CLexTokenString::~CLexTokenString()
    {
    }

    class CLexTokenInt : public CLexToken
    {
    public:
        CLexTokenInt (unsigned int token_data);
        virtual ~CLexTokenInt ();
        virtual unsigned int GetInt() { return m_TokenData; };
    private:
        unsigned int m_TokenData;
    };

    CLexTokenInt::CLexTokenInt(unsigned int token_data) : CLexToken (e_Int)
    {
        m_TokenData = token_data;
    }

    CLexTokenInt::~CLexTokenInt()
    {
    }

    class CLexTokenAccession : public CLexToken {
    public:
        CLexTokenAccession (const string &token_data);
        virtual ~CLexTokenAccession();
        virtual string GetString(void) { return m_TokenData; };
    private:
        string m_TokenData;
    };

    CLexTokenAccession::CLexTokenAccession( const string &token_data )
        : CLexToken(e_Accession), m_TokenData(token_data) 
    {
    }

    CLexTokenAccession::~CLexTokenAccession()
    {
    }

    class CLexTokenParenPair : public CLexToken
    {
    public:
        CLexTokenParenPair (unsigned int token_type, string between_text);
        virtual ~CLexTokenParenPair();

        virtual CRef<CSeq_loc> GetLocation(CSeq_id *id, CGetSeqLocFromStringHelper* helper);

        static CRef<CSeq_loc> ReadLocFromTokenList (TLexTokenArray token_list, CSeq_id *id, CGetSeqLocFromStringHelper* helper);

    private:
        TLexTokenArray m_TokenList;
    };

    CLexTokenParenPair::CLexTokenParenPair(unsigned int token_type, string between_text) : CLexToken (token_type)
    {
        m_TokenList.clear();
        m_HasError = ! s_ParseLex (between_text, m_TokenList);
    }

    CLexTokenParenPair::~CLexTokenParenPair()
    {
    }

    CRef<CSeq_loc> CLexTokenParenPair::GetLocation(CSeq_id *id, CGetSeqLocFromStringHelper* helper)
    {
        CRef<CSeq_loc> retval = ReadLocFromTokenList(m_TokenList, id, helper);
        
        if (m_TokenType == e_Complement) {
            retval = helper->GetRevComplement(*retval);
        }
        return retval;
    }

    CRef<CSeq_loc> CLexTokenParenPair::ReadLocFromTokenList (TLexTokenArray token_list, CSeq_id *this_id, CGetSeqLocFromStringHelper* helper)
    {
        CRef<CSeq_id> id( this_id );

        CRef<CSeq_loc> retval;
        CRef<CSeq_loc> add;
        unsigned int list_pos;
        TLexTokenArray before_comma_list;
        vector <unsigned int> comma_pos;
        
        retval.Reset();
        if (token_list.size() < 1) {
            return retval;
        }
            
        comma_pos.clear();
        for (list_pos = 0; list_pos < token_list.size(); list_pos++) {
            if (token_list[list_pos]->GetTokenType() == CLexToken::e_Comma) {
                comma_pos.push_back (list_pos);
            }
        }
        
        if (comma_pos.size() > 0) {
            retval = new CSeq_loc ();
            list_pos = 0;
            for (unsigned int k = 0; k < comma_pos.size(); k++) {
                before_comma_list.clear();
                while (list_pos < comma_pos[k]) {
                    before_comma_list.push_back (token_list[list_pos]);
                    list_pos++;
                }
                add = ReadLocFromTokenList(before_comma_list, id, helper);
                if (add == NULL) {
                    retval.Reset();
                    return retval;
                } else {
                    if( retval->Which() == CSeq_loc::e_not_set ) {
                        retval.Reset( new CSeq_loc );
                        retval->Assign( *add );
                    } else {
                        retval = helper->Seq_loc_Add (*retval, *add, 0);
                    }
                }
                // skip over comma
                list_pos ++;
            }
            before_comma_list.clear();
            while (list_pos < token_list.size()) {
                before_comma_list.push_back (token_list[list_pos]);
                list_pos++;
            }
            add = ReadLocFromTokenList(before_comma_list, id, helper);
            if( retval->Which() == CSeq_loc::e_not_set ) {
                retval.Reset( new CSeq_loc );
                retval->Assign( *add );
            } else {
                retval = helper->Seq_loc_Add (*retval, *add, 0);
            }
            return retval;
        } else {    
            
            switch (token_list[0]->GetTokenType()) {
                case CLexToken::e_Accession:
                    id = new CSeq_id( token_list[0]->GetString() );
                    token_list.erase( token_list.begin() ); // inefficient
                    // !!!!!FALL-THROUGH!!!!!
                case CLexToken::e_Int:
                    if (token_list.size() == 1) {
                        // note - subtract one from the int read, because display is 1-based
                        retval = new CSeq_loc (*id, token_list[0]->GetInt() - 1);
                    } else if (token_list[1]->GetTokenType() == CLexToken::e_DotDot) {
                        if (token_list.size() < 3 || token_list[2]->GetTokenType() != CLexToken::e_Int) {
                            retval.Reset();
                            return retval;
                        }
                        if (token_list.size() > 4) {
                            retval.Reset();
                            return retval;
                        }
                        if (token_list.size() == 4 && token_list[3]->GetTokenType() != CLexToken::e_RightPartial) {
                            retval.Reset();
                            return retval;
                        }
                        // note - subtract one from the int read, because display is 1-based
                        retval = new CSeq_loc (*id, token_list[0]->GetInt() - 1, token_list[2]->GetInt() - 1);
                        if (token_list.size() == 4) {
                            retval->SetPartialStop(true, eExtreme_Positional);
                        }
                    }
                    break;
                case CLexToken::e_LeftPartial:
                    if (token_list.size() < 2) {
                        retval.Reset();
                        return retval;
                    } else if (token_list.size() == 2) {
                        // note - subtract one from the int read, because display is 1-based
                        retval = new CSeq_loc (*id, token_list[1]->GetInt() - 1);
                        retval->SetPartialStart(true, eExtreme_Positional);
                    } else if (token_list[2]->GetTokenType() == CLexToken::e_DotDot) {
                        if (token_list.size() < 4 || token_list[3]->GetTokenType() != CLexToken::e_Int) {
                            retval.Reset();
                            return retval;
                        }
                        if (token_list.size() > 5) {
                            retval.Reset();
                            return retval;
                        }
                        if (token_list.size() == 5 && token_list[4]->GetTokenType() != CLexToken::e_RightPartial) {
                            retval.Reset();
                            return retval;
                        }
                        // note - subtract one from the int read, because display is 1-based
                        retval = new CSeq_loc (*id, token_list[1]->GetInt() - 1, token_list[3]->GetInt() - 1);
                        retval->SetPartialStart(true, eExtreme_Positional);
                        if (token_list.size() == 5) {
                            retval->SetPartialStop(true, eExtreme_Positional);
                        }
                    }
                    break;
                
                case CLexToken::e_ParenPair:
                case CLexToken::e_Join:
                case CLexToken::e_Order:
                case CLexToken::e_Complement:
                    if (token_list.size() > 1) {
                        retval.Reset();
                        return retval;
                    }
                    retval = token_list[0]->GetLocation(id, helper);
                    break;
                case CLexToken::e_String:
                    break;
                case CLexToken::e_DotDot:
                    break;
                case CLexToken::e_RightPartial:
                     break;
                case CLexToken::e_Comma:
                    break;
                default:
                    break;
            }
        }
        return retval;
    }

    void s_RemoveWhiteSpace(string& str)
    {
        string copy;
        unsigned int pos;
        
        for (pos = 0; pos < str.length(); pos++) {
            if (!isspace((unsigned char) str[pos]) && (str[pos] != '~')) {
                copy += str.substr(pos, 1);
            }
        }
        
        str = copy;
    }

    unsigned int s_GetParenLen (string text)
    {
        string::size_type offset = 0;
        unsigned int paren_count;
        string::size_type next_quote;

        if (!NStr::StartsWith(text, "(")) {
            return 0;
        }
        
        offset++;
        paren_count = 1;
        
        while (offset != text.length() && paren_count > 0) {
            if (NStr::StartsWith(text.substr(offset), "(")) {
                paren_count ++;
                offset++;
            } else if (NStr::StartsWith(text.substr(offset), ")")) {
                paren_count --;
                offset++;
            } else if (NStr::StartsWith(text.substr(offset), "\"")) {
                // skip quoted text
                offset++;
                next_quote = NStr::Find(text, "\"", offset);
                if (next_quote == string::npos) {
                    return 0;
                } else {
                    offset = next_quote + 1;
                }
            } else {
                offset++;
            }            
        }
        if (paren_count > 0) {
            return 0;
        } else {
            return offset;
        }
    }

    bool s_ParseLex (string text, TLexTokenArray &token_list)
    {
        char ch;
        bool retval = true;
        string::size_type paren_len, offset = 0, end_pos;
            
        if (NStr::IsBlank(text)) {
            return false;
        }

        s_RemoveWhiteSpace(text);
        
        while (offset < text.length() && retval) {
            ch = text.c_str()[offset];
            switch ( ch) {

                case '\"':
                    // skip to end of quotation
                    end_pos = NStr::Find(text, "\"", offset + 1);
                    if (end_pos == string::npos) {
                        retval = false;
                    } else {
                        token_list.push_back (new CLexTokenString (text.substr (offset, end_pos - offset + 1)));
                        offset = end_pos + 1;
                    }
                    break;
    /*------
     *  NUMBER
     *------*/
			    case '0': case '1': case '2': case '3': case '4':
			    case '5': case '6': case '7': case '8': case '9':
			        end_pos = offset + 1;
			        while (end_pos < text.length() && isdigit (text.c_str()[end_pos])) {
			            end_pos ++;
			        }
				    token_list.push_back (new CLexTokenInt (NStr::StringToInt(text.substr(offset, end_pos - offset))));
				    offset = end_pos;
				    break;
    // parentheses
                case '(':
                    paren_len = s_GetParenLen(text.substr(offset));
                    if (paren_len == 0) {
                        retval = false;
                    } else {
                        token_list.push_back (new CLexTokenParenPair (CLexToken::e_ParenPair, text.substr(offset + 1, paren_len - 2)));
                        if (token_list[token_list.size() - 1]->HasError()) {
                            retval = false;
                        }
                        offset += paren_len;
                    }
                    break;				
    /*------
     *  JOIN
     *------*/
			    case 'j':
				    if (NStr::EqualNocase (text.substr(offset, 4), "join")) {
				        offset += 4;
				        paren_len = s_GetParenLen(text.substr(offset));
				        if (paren_len == 0) {
				            retval = false;
				        } else {
				            token_list.push_back (new CLexTokenParenPair (CLexToken::e_Join, text.substr(offset + 1, paren_len - 2)));
				        }				    
                        offset += paren_len;
				    } else {
				        retval = false;
				    }
				    break;
    			
    /*------
     *  ORDER
     *------*/
			    case 'o':
				    if (NStr::EqualNocase (text.substr(offset, 5), "order")) {
				        offset += 5;
				        paren_len = s_GetParenLen(text.substr(offset));
				        if (paren_len == 0) {
				            retval = false;
				        } else {
				            token_list.push_back (new CLexTokenParenPair (CLexToken::e_Order, text.substr(offset + 1, paren_len - 2)));
				        }				    
				    } else {
				        retval = false;
				    }
				    break;
    /*------
     *  COMPLEMENT
     *------*/
			    case 'c':
				    if (NStr::EqualNocase (text.substr(offset, 10), "complement")) {
				        offset += 10;
				        paren_len = s_GetParenLen(text.substr(offset));
				        if (paren_len == 0) {
				            retval = false;
				        } else {
				            token_list.push_back (new CLexTokenParenPair (CLexToken::e_Complement, text.substr(offset + 1, paren_len - 2)));
				        }	
                        offset += paren_len;
				    } else {
				        retval = false;
				    }
				    break;
                case '-':
                    token_list.push_back (new CLexToken (CLexToken::e_DotDot));
                    offset++;
				    break;
			    case '.':
				    if (NStr::Equal(text.substr(offset, 2), "..")) {
				        token_list.push_back (new CLexToken (CLexToken::e_DotDot));
				        offset += 2;
				    } else {
				        retval = false;
				    }
				    break;
                case '>':
                    token_list.push_back (new CLexToken (CLexToken::e_RightPartial));
                    offset ++;
				    break;
                case '<':
                    token_list.push_back (new CLexToken (CLexToken::e_LeftPartial));
                    offset ++;
				    break;
                case ';':
                case ',':
                    token_list.push_back (new CLexToken (CLexToken::e_Comma));
                    offset ++;
				    break;	
                case 't' :
				    if (NStr::Equal(text.substr(offset, 2), "to")) {
				        token_list.push_back (new CLexToken (CLexToken::e_DotDot));
				        offset += 2;
				    } else {
				        retval = false;
				    }
				    break;
                default:
    // ACCESSION
    // (accessions start with a capital letter, then numbers then
    //  an optional version prefix, then a colon)
                    if( isupper(ch) ) {
                        end_pos = offset + 1;
                        while (end_pos < text.length() && isupper (text.c_str()[end_pos])) {
                            end_pos++;
                        }
                        while (end_pos < text.length() && isdigit (text.c_str()[end_pos])) {
                            end_pos++;
                        }
                        if( text.c_str()[end_pos] == '.' ) {
                            ++end_pos;
                            while (end_pos < text.length() && isdigit (text.c_str()[end_pos])) {
                                end_pos++;
                            }
                        }
                        if( text.c_str()[end_pos] != ':' ) {
                            retval = false;
                        }
                        ++end_pos;
                        token_list.push_back (new CLexTokenAccession (text.substr(offset, end_pos - offset - 1))); // "- 1" to ignore colon
                        offset = end_pos;
                    } else {
                        retval = false;
                    }
                    break;
            }
        }
    				
        return retval;			
    }
}

CGetSeqLocFromStringHelper::~CGetSeqLocFromStringHelper(void)
{
    // do nothing
}

CRef<CSeq_loc> 
CGetSeqLocFromStringHelper::GetRevComplement(const CSeq_loc& loc)
{
    CReverseComplementHelper helper;
    return CRef<CSeq_loc>(GetReverseComplement( loc, &helper ));
}

CRef<CSeq_loc>
CGetSeqLocFromStringHelper::Seq_loc_Add(
        const CSeq_loc&    loc1,
        const CSeq_loc&    loc2,
        CSeq_loc::TOpFlags flags )
{
    // No ISynonymMapper due to lack of a CScope
    return loc1.Add(loc2, flags, NULL);
}
 
CRef<CSeq_loc> GetSeqLocFromString(
    const string &text, const CSeq_id *id, CGetSeqLocFromStringHelper *helper)
{
    CRef<CSeq_loc> retval(NULL);
    TLexTokenArray token_list;

    token_list.clear();

    CRef<CSeq_id> this_id(new CSeq_id());
    this_id->Assign(*id);


    if (s_ParseLex (text, token_list)) {
        retval = CLexTokenParenPair::ReadLocFromTokenList (token_list, this_id, helper);
    }

    return retval;
}

END_SCOPE(objects)
END_NCBI_SCOPE
