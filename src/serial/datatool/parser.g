#header <<
#include <iostream>
#include <vector>
#include <string>
#define PURIFY(var,size)
using namespace std;
#include "asnmodule.hpp"
#include "asntype.hpp"
#include "asnvalue.hpp"
>>

<<
typedef ANTLRCommonToken ANTLRToken;
#include "DLGLexer.h"
#include "PBlackBox.h"

int main()
{
	ParserBlackBox<DLGLexer, ASNParser, ANTLRToken> p(stdin);
	p.parser()->parse();
    p.parser()->Print(cout);
	return 0;
}

>>

#lexclass START
#token "[\ \t\r]+"	<< skip(); >>
#token "\n"         << newline(); skip(); >>
#token "\-\-"       << mode(COMMENT); skip(); >>
#token "\""         << mode(STRING); skip(); >>
#token "\'"         << mode(BSTRING); skip(); >>

#token T_DEFINE "::="
#token T_L_BRACE "\{"
#token T_R_BRACE "\}"
#token T_SEMICOLON ";"
#token T_COMMA ","
#token T_L_PARENTHESIS "\("
#token T_R_PARENTHESIS "\)"
#token T_MINUS "\-"

#token K_DEFINITIONS "DEFINITIONS"
#token K_BEGIN "BEGIN"
#token K_END "END"
#token K_EXPORTS "EXPORTS"
#token K_IMPORTS "IMPORTS"
#token K_FROM "FROM"
#token K_SET "SET"
#token K_SEQUENCE "SEQUENCE"
#token K_CHOICE "CHOICE"
#token K_OF "OF"
#token K_INTEGER "INTEGER"
#token K_REAL "REAL"
#token K_BOOLEAN "BOOLEAN"
#token K_VisibleString "VisibleString"
#token K_StringStore "StringStore"
#token K_ENUMERATED "ENUMERATED"
#token K_OCTET "OCTET"
#token K_BIT "BIT"
#token K_STRING "STRING"
#token K_NULL "NULL"
#token K_TRUE "TRUE"
#token K_FALSE "FALSE"
#token K_OPTIONAL "OPTIONAL"
#token K_DEFAULT "DEFAULT"
#token K_IMPLICIT "IMPLICIT"

#token IDENTIFIER "[a-zA-Z_]([a-zA-Z_0-9]|\-[a-zA-Z_0-9])*"
#token NUMBER "[0-9]+"

#lexclass COMMENT
#token "\-\-"         << mode(START); skip(); >>
#token "\n"         << newline(); mode(START); skip(); >>
#token "~[]"        << skip(); >>
#token "@"          << mode(START); >>

#lexclass STRING
#token "\\n" << replchar('\n'); more(); >>
#token "\\r" << replchar('\r'); more(); >>
#token "\\b" << replchar('\b'); more(); >>
#token "\\t" << replchar('\t'); more(); >>
#token "\\{\r}\n" << replchar('\0'); newline(); more(); >>
#token "\\[0-3]{[0-9]}{[0-9]}"
    <<
        DLGChar c = '0';
        for ( DLGChar* p = begexpr() + 1, *end = endexpr(); p < end; ++p )
            c = (c << 3) | (*p - '0');
        replchar(c); more();
    >>
#token BADSTRING "({\r}[\n@]|\\@)"
    << replchar('\0'); Warning("unclosed string"); newline(); mode(START); >>
#token GOODSTRING "\""   << replchar('\0'); mode(START); >>
#token "~[]" << more(); >>

#tokclass STRING { GOODSTRING BADSTRING }

#lexclass BSTRING
#token "[0-9A-Z]+" << more(); >>
#token OPENBSTRING "{\r}[\n@]"
    << Warning("unclosed bit string"); newline(); replchar(0); mode(START); >>
#token GOODBITSTRING  "\'B" << replchar(0); mode(START); >>
#token GOODHEXSTRING  "\'H" << replchar(0); mode(START); >>
#token BADBSTRING "\'"
    << Warning("iilegal bit string type"); replchar(0); mode(START); >>
#token "[]"
    << Warning("bad char in bit string"); replchar(0); more(); >>

#tokclass BITSTRING { GOODBITSTRING BADBSTRING OPENBSTRING }
#tokclass HEXSTRING { GOODHEXSTRING }

class ASNParser {
<<
public:

    typedef list<AutoPtr<ASNModule> > TModules;

ostream& Print(ostream& out) const
{
    for ( TModules::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        (*i)->Print(out);
    }
    return out;
}

TModules modules;

private:
>>

parse:
    << AutoPtr<ASNModule> module; >>
    (
        moduleDefinition>[module]
        << modules.push_back(module); >>
    )*
    "@"
;

moduleDefinition>[AutoPtr<ASNModule> module]:
    << $module = new ASNModule; >>
    identifier>[$module->name]
    K_DEFINITIONS
    T_DEFINE
    K_BEGIN
    {
        K_EXPORTS exports[$module]
    }
    {
        K_IMPORTS imports[$module]
    }
    moduleBody[$module]
    <<
        if ( !$module->Check() )
            cerr << "Errors was found" << endl;
    >>
    K_END
;

exports[AutoPtr<ASNModule>& module]:
    idlist[$module->exports]
    T_SEMICOLON
;

imports[AutoPtr<ASNModule>& module]:
    (
        (
            << AutoPtr<ASNModule::Import> imp = new ASNModule::Import; >>
            idlist[imp->types] K_FROM identifier>[imp->module]
            << $module->imports.push_back(imp); >>
        )
    )+ T_SEMICOLON
;

moduleBody[AutoPtr<ASNModule>& module]:
    (
        << string id; AutoPtr<ASNType> t; >>
        identifier>[id]
        T_DEFINE
        type>[t]
        << $module->AddDefinition(id, t); >>
    )*
;

type>[AutoPtr<ASNType> t]:
    << int line = LT(1)->getLine(); >>
(
    K_BOOLEAN << $t = new ASNBooleanType; >>
|
    K_INTEGER
    (
        enumeratedBlock[$t, "INTEGER"]
    |
        << $t = new ASNIntegerType; >>
    )
|
    K_ENUMERATED enumeratedBlock[$t, "ENUMERATED"]
|
    K_REAL << $t = new ASNRealType; >>
|
    K_BIT K_STRING << $t = new ASNBitStringType; >>
|
    K_OCTET K_STRING << $t = new ASNOctetStringType; >>
|
    K_NULL << $t = new ASNNullType; >>
|
    K_SEQUENCE sequenceType>[$t]
|
    K_SET setType>[$t]
|
    K_CHOICE
    (
        << ASNChoiceType* choice; $t = choice = new ASNChoiceType; >>
        typesBlock[choice->members]
    )
|
    K_VisibleString << $t = new ASNVisibleStringType; >>
|
    K_StringStore << $t = new ASNStringStoreType; >>
|
    (
        << string name; >>
        identifier>[name]
        << $t = new ASNUserType(name); >>
    )
)
    << $t->line = line; >>
;

setType>[AutoPtr<ASNType> t]:
    K_OF
    (
        type>[$t]
        << $t = new ASNSetOfType($t); >>
    )
|
    (
        << ASNSetType* s; $t = s = new ASNSetType; >>
        typesBlock[s->members]
    )
;

sequenceType>[AutoPtr<ASNType> t]:
    K_OF
    (
        type>[$t]
        << $t = new ASNSequenceOfType($t); >>
    )
|
    (
        << ASNSequenceType* s; $t = s = new ASNSequenceType; >>
        typesBlock[s->members]
    )
;

typesBlock[list<AutoPtr<ASNMember> >& members]:
    T_L_BRACE
        namedDataType[members]
        (
            T_COMMA namedDataType[members]
        )*
    T_R_BRACE
;

namedDataType[list<AutoPtr<ASNMember> >& members]:
    << AutoPtr<ASNMember> member = new ASNMember; >>
    identifier>[member->name]
    type>[member->type]
    (   // attributes
        K_OPTIONAL
        << member->optional = true; >>
    |
        K_DEFAULT value>[member->defaultValue]
    |
        // none
    )
    << members.push_back(member); >>
;

enumeratedBlock[AutoPtr<ASNType>& t, const string& keyword]:
    << ASNEnumeratedType* e; $t = e = new ASNEnumeratedType(keyword); >>
    T_L_BRACE
        enumeratedValue[e]
        (
            T_COMMA enumeratedValue[e]
        )*
    T_R_BRACE
;

enumeratedValue[ASNEnumeratedType* t]:
    << string id; >>
    identifier>[id]
    (
        << long value; >>
        T_L_PARENTHESIS
            number>[value]
        T_R_PARENTHESIS
        << $t->AddValue(id, value); >>
    |
        << $t->AddValue(id); >>
    )
;

value>[AutoPtr<ASNValue> v]:
    << int line = LT(1)->getLine(); >>
(
    (
        << long n; >>
        number>[n]
        << $v = new ASNIntegerValue(n); >>
    )
|
    (
        << string s; >>
        stringValue>[s]
        << $v = new ASNStringValue(s); >>
    )
|
    K_NULL
    << $v = new ASNNullValue; >>
|
    K_FALSE
    << $v = new ASNBoolValue(false); >>
|
    K_TRUE
    << $v = new ASNBoolValue(true); >>
|
    T_L_BRACE
    (
        << ASNBlockValue* b; $v = b = new ASNBlockValue; >>
        valueBlock[b->values]
    )
    T_R_BRACE
|
    (
        << string id; >>
        identifier>[id]
        (
            (
                value>[$v]
                << $v = new ASNNamedValue(id, $v); >>
            )
        |
            << $v = new ASNIdValue(id); >>
        )
    )
)
    << $v->line = line; >>
;

valueBlock[list<AutoPtr<ASNValue> >& values]:
    {
        << AutoPtr<ASNValue> v; >>
        value>[v] << values.push_back(v); >>
        (
            T_COMMA
            value>[v] << values.push_back(v); >>
        )*
    }
;

number>[long value]:
    (
        T_MINUS text1:NUMBER
        << $value = -atol($text1->getText()); >>
    |
        text2:NUMBER
        << $value = atol($text2->getText()); >>
    )
;

identifier>[string value]:
    text:IDENTIFIER
    << $value = $text->getText(); >>
;

idlist[list<string>& ids]:
    << string id; >>
    identifier>[id]
    << $ids.push_back(id); >>
    (
        T_COMMA identifier>[id]
        << $ids.push_back(id); >>
    )*
;

stringValue>[string value]:
    text:STRING
    << $value = $text->getText(); >>
;

}
