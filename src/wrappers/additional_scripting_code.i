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
 * File Description:  Perl and Python code for insertion
 *
 */


#ifdef SWIGPYTHON

%pythoncode %{

# Without this, object manager use gives a crash
# if SYBASE environment variable is set
import os
os.environ['GENBANK_LOADER_METHOD'] = 'id1'

# dynamic casting
def dynamic_cast(new_class, obj):
    return new_class.__dynamic_cast_to__(obj)


# Downcast an object as far down as possible;
# may be very slow
def Downcast(obj):
    for subclass in obj.__class__.__subclasses__():
        if not subclass.__subclasses__():
            # a *Ptr class
            continue
        dc = dynamic_cast(subclass, obj)
        if dc:
            return Downcast(dc)
    # no successful downcasts
    return obj


# Downcast a CSerialObject as far as possible,
# based on type info.  This should be faster
# than Downcast().
def DowncastSerialObject(so):
    obj_type_name = so.GetThisTypeInfo().GetName()
    cname = 'C' + obj_type_name.replace('-', '_')
    # class need not be in this module; look through all modules
    import sys
    klass = None
    for key in sys.modules.keys():
        d = sys.modules[key].__dict__
        if d.has_key(cname):
            if issubclass(d[cname], CSerialObject):
                klass = d[cname]
                break
    if klass == None:
        raise RuntimeError, 'could not downcast to %s: class not found' \
              % cname
    return dynamic_cast(klass, so)


# read any asn or xml file

def ReadAsnFile(fname, obj_type = None):
    '''
    Read a text ASN.1 file containing an object of any type known to this
    code module.  The ASN.1 type can optionally be specified in the
    obj_type parameter.  If it is omitted, the type will be determined
    from the file header.  If supplied, it must match the actual type of
    the object in the file or an exception will be raised.

    examples:

    obj = ReadAsnFile('myfile.prt')                       # reads any type
    ch22 = ReadAsnFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.prt', 'Seq-entry') # expects a Seq-entry
    '''
    return ReadSerialFile(fname, eSerial_AsnText, obj_type)

def ReadXmlFile(fname, obj_type = None):
    '''
    Read an xml file containing an ncbi serial object of any type known
    to this code module.  The serial type can optionally be specified in
    the obj_type parameter.  If it is omitted, the type will be determined
    from the file header.  If supplied, it must match the actual type of
    the object in the file or an exception will be raised.

    examples:

    obj = ReadXmlFile('myfile.xml')                       # reads any type
    ch22 = ReadXmlFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.xml', 'Seq-entry') # expects a Seq-entry
    '''
    return ReadSerialFile(fname, eSerial_Xml, obj_type)

def ReadAsnBinFile(fname, obj_type):
    '''
    Read a binary ASN.1 file containing an object of any type known to
    this code module.  The ASN.1 type MUST be specified in the obj_type
    parameter.

    example:

    # read a Seq-entry stored in ASN.1 binary
    ch22 = ReadAsnBinFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.val', 'Seq-entry')
    '''
    return ReadSerialFile(fname, eSerial_AsnBinary, obj_type)

def ReadSerialFile(fname, serial_format, obj_type = None):
    '''
    Read a file containing an ncbi serial object in text or binary ASN.1
    or xml.  The serial_format parameter is eSerial_AsnText,
    eSerial_AsnBinary, or eSerial_Xml.  obj_type is a string giving
    the object type (e.g., 'Seq-loc').  It may be omitted for text ASN.1
    and xml, but must be supplied for ASN.1 binary.

    examples:
    
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.prt', eSerial_AsnText, 'Seq-entry')
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.val', eSerial_AsnBinary, 'Seq-entry')
    ch22 = ReadSerialFile('/netopt/ncbi_tools/c++-wrappers/data/chromosome22.xml', eSerial_Xml, 'Seq-entry')
    obj = ReadSerialFile('myfile.prt', eSerial_AsnText)
    obj = ReadSerialFile('myfile.xml', eSerial_Xml)
    '''
    objistr = CObjectIStream.Open(serial_format, fname)
    if obj_type == None:
        if serial_format == eSerial_AsnBinary:
            raise RuntimeError, 'ASN.1 type must be specified for read of ' \
                               'binary file %s' % fname
        obj_type_name = objistr.ReadFileHeader()
    else:
        obj_type_name = obj_type
    cname = 'C' + obj_type_name.replace('-', '_')
    # class need not be in this module; look through all modules
    import sys
    obj = None
    for key in sys.modules.keys():
        d = sys.modules[key].__dict__
        if d.has_key(cname):
            if issubclass(d[cname], CSerialObject):
                obj = d[cname]()	
                break
    if obj == None:
        raise RuntimeError, 'class %s not found while reading a %s' \
              % (cname, obj_type_name)

    if obj_type == None:
        objistr.Read(obj, obj.GetThisTypeInfo(), objistr.eNoFileHeader)
    else:
        objistr.Read(obj, obj.GetThisTypeInfo())
    objistr.Close()
    return obj


# tree view of serial objects using kxmleditor
def SerialObjectTreeView(sobj, *args):
    import os, tempfile
    caption_switch = ''
    if len(args) > 0:
        if len(args) > 1:
            raise "takes 1 or 2 arguments: %d given" % len(args + 1)
        caption_switch = '--caption \'' + args[0] + '\' '
    fname = tempfile.mktemp()
    fid = open(fname, 'w')
    fid.write(sobj.Xml())
    fid.close()
    os.system('kxmleditor %s%s >& /dev/null || '
              '/usr/local/kxmleditor/bin/kxmleditor %s >& /dev/null &'
               % (caption_switch, fname, fname) )

CSerialObject.TreeView = SerialObjectTreeView


def SerialObjectTextView(sobj):
    CTextEditor.DisplayText(sobj.Asn())

CSerialObject.TextView = SerialObjectTextView

def SerialObjectTextEdit(sobj):
    s = string()
    CTextEditor.EditText(sobj.Asn(), s)
    rv = sobj.__class__()
    rv.FromAsn(s)
    return rv

CSerialObject.TextEdit = SerialObjectTextEdit


def WithinNcbi():
    '''
    Determine from ip address whether we're running within NCBI.
    NCBI machines have addresses of the form 130.14.2x.zzz
    '''
    try:
        ip_int = CSocketAPI.gethostbyname(CSocketAPI.gethostname())
        ip = [(ip_int >> 8 * i) & 255 for i in range(4)]
        if ip[0] == 130 and ip[1] == 14 and ip[2] in range(20, 30):
            return True
    except:
        pass
    return False


if WithinNcbi():
    doxy_url_base = 'http://intranet.ncbi.nlm.nih.gov/' \
                    'ieb/ToolBox/CPP_DOC/doxyhtml/'
else:
    doxy_url_base = 'http://www.ncbi.nlm.nih.gov/' \
                    'ieb/ToolBox/CPP_DOC/doxyhtml/'

def Doxy(arg):
    '''
    Show Doxygen-generated documentation of class in a web browser
    '''
    if isinstance(arg, type):
        # a Python class
        class_name = arg.__name__
    elif isinstance(arg, str):
        # a string?
        class_name = arg
    elif isinstance(arg, object):
        # an instance of a "new-style" class
        class_name = arg.__class__.__name__
    else:
        raise "Doxy: couldn't figure out a class name from the argument"

    mangled_class_name = class_name.replace('_', '__')
    if class_name[0] == 'S':
        class_or_struct = 'struct'
    else:
        class_or_struct = 'class'
    import webbrowser
    url = doxy_url_base + class_or_struct + mangled_class_name + '.html'
    # on unix, if DISPLAY not set, remove some graphical browsers from
    # consideration (they may not return non-zero status when they fail,
    # so the user sees nothing)
    import os
    orig_tryorder = webbrowser._tryorder
    if os.name == 'posix':
       if not os.environ.has_key('DISPLAY'):
          webbrowser._tryorder = []
          for br in orig_tryorder:
             if not br in ['mozilla', 'netscape']:
                webbrowser._tryorder.append(br)
                
    webbrowser.open_new(url)
    webbrowser._tryorder = orig_tryorder

ncbi_object.Doxy = classmethod(Doxy)


# Similar for LXR
if WithinNcbi():
    lxr_url_base = 'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/'
else:
    lxr_url_base = 'http://www.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/'

def Lxr(arg):
    '''
    Show LXR-generated documentation of class in a web browser
    '''
    if isinstance(arg, type):
        # a Python class
        class_name = arg.__name__
    elif isinstance(arg, str):
        # a string?
        class_name = arg
    elif isinstance(arg, object):
        # an instance of a "new-style" class
        class_name = arg.__class__.__name__
    else:
        raise "Lxr: couldn't figure out a symbol name from the argument"

    import webbrowser
    url = lxr_url_base + 'ident?i=' + class_name
    # on unix, if DISPLAY not set, remove some graphical browsers from
    # consideration (they may not return non-zero status when they fail,
    # so the user sees nothing)
    import os
    orig_tryorder = webbrowser._tryorder
    if os.name == 'posix':
       if not os.environ.has_key('DISPLAY'):
          webbrowser._tryorder = []
          for br in orig_tryorder:
             if not br in ['mozilla', 'netscape']:
                webbrowser._tryorder.append(br)
                
    webbrowser.open_new(url)
    webbrowser._tryorder = orig_tryorder

ncbi_object.Lxr = classmethod(Lxr)

# Doxygen search; take a string, bring up url that searches for it.
def DoxySearch(name):
    '''
    Search Doxygen-generated documentation for a name,
    showing result in a web browser
    '''
    import webbrowser
    url = doxy_url_base + 'search.php?query=' + name
    # on unix, if DISPLAY not set, remove some graphical browsers from
    # consideration (they may not return non-zero status when they fail,
    # so the user sees nothing)
    import os
    orig_tryorder = webbrowser._tryorder
    if os.name == 'posix':
       if not os.environ.has_key('DISPLAY'):
          webbrowser._tryorder = []
          for br in orig_tryorder:
             if not br in ['mozilla', 'netscape']:
                webbrowser._tryorder.append(br)
                
    webbrowser.open_new(url)
    webbrowser._tryorder = orig_tryorder


try:
	import find_asn_spec
except:
	pass

asn_spec_url_base = 'http://graceland.ncbi.nlm.nih.gov:6224' \
                    '/staff/jcherry/asn_spec/'
def Spec(arg, web=False):
   if isinstance(arg, CSerialObject):
      # a serial object; get type string from type info
      type = arg.GetThisTypeInfo().GetName()
   else:
      # a string, one hopes
      type = arg
   if not web:
      spec = find_asn_spec.FindSpec(type)
      if spec == None:
         print 'ASN.1 spec. not found for type %s' % type
      else:
         print spec
   else:
      url = asn_spec_url_base + type + '.html'
      import webbrowser
      # on unix, if DISPLAY not set, remove some graphical browsers from
      # consideration (they may not return non-zero status when they fail,
      # so the user sees nothing)
      import os
      orig_tryorder = webbrowser._tryorder
      if os.name == 'posix':
         if not os.environ.has_key('DISPLAY'):
            webbrowser._tryorder = []
            for br in orig_tryorder:
               if not br in ['mozilla', 'netscape']:
                  webbrowser._tryorder.append(br)
      webbrowser.open_new(url)
      
CSerialObject.Spec = Spec


def EntrezUrl(ids, db=None):

    def GetGi(id, id1cli):
        if isinstance(id, int):
            return id
        if isinstance(id, string):
            id = str(id)
        if isinstance(id, str):
            try:
                gi = int(id)
                return gi
            except:
                seq_id = CSeq_id(id)
        elif isinstance(id, CSeq_id):
            seq_id = id
        else:
            raise "couldn't convert argument to a gi"

        return id1cli.AskGetgi(seq_id)

    id1cli = CID1Client()

    if isinstance(ids, str) or isinstance(ids, int) \
           or isinstance(ids, string) \
           or isinstance(ids, CSeq_id):
        ids = [ids]

    ids = ids[:]   # in case we have a std::list, which can't be indexed
    if db == None:
        # Assume it should be 'protein' or 'nucleotide';
        # figure out which from the *first* gi
        hand = CSimpleOM.GetBioseqHandle(GetGi(ids[0], id1cli))
        if hand.GetBioseqMolType() == CSeq_inst.eMol_aa:
            db = 'protein'
        else:
            db = 'nucleotide'
        
    base_url = 'http://www.ncbi.nlm.nih.gov/entrez'
    query_start = '/query.fcgi?db=%s&cmd=Retrieve&list_uids=' % db
    url = base_url + query_start
    for i in range(0, len(ids)):
        if i > 0:
            url += ','
        url += str(GetGi(ids[i], id1cli))
    return url


def EntrezWeb(ids, db=None):
    import webbrowser
    webbrowser.open_new(EntrezUrl(ids, db))


# Search ncbi namespace for names containing a pattern
import re
def Search(pattern, deep=False):
    names = globals().keys()
    names.sort()
    for name in names:
        if re.search(pattern.lower(), name.lower()):
            if name.endswith('__dynamic_cast_to__'):
                continue
            if name.endswith('Ptr') and name[:-3] in names:
                # filter out SWIG Ptr classes
                continue
            print name
        if deep and type(globals()[name]) == type(object):
            for mem_name in globals()[name].__dict__.keys():
                if re.search(pattern.lower(), mem_name.lower()):
                    print '%s.%s' % (name, mem_name)


def _class_monitoring_init(self, *args):
    global class_monitor_info
    cname = self.__class__.__name__
    if cname.endswith('Ptr') and len(self.__class__.__subclasses__()) == 0:
        cname = cname[0:-3]
    if cname in class_monitor_info:
        class_monitor_info[cname] += 1
    else:
        class_monitor_info[cname] = 1
    self.__orig_init__(*args)


def recursive_modify(base_class):
    if not base_class.__dict__.has_key('__orig_init__'):
        # modify only if we haven't already been here
        # (multiple inheritance issue)
        base_class.__orig_init__ = base_class.__init__
        base_class.__init__ = _class_monitoring_init
    for sub_class in base_class.__subclasses__():
        recursive_modify(sub_class)
 

def enable_class_monitoring():
    global class_monitor_info
    class_monitor_info = {}
    recursive_modify(ncbi_object)    


# no-op implementation to ease switching between ncbi and ncbi_fast
def use_class(arg):
    pass


#del object
%}

#endif

#ifdef SWIGPERL

%pragma(perl5) code = %{

# Without this, object manager use gives a crash
# if SYBASE environment variable is set
$ENV{'GENBANK_LOADER_METHOD'} = 'id1';

############ dynamic_cast() -- Dynamic typecasting ############

package ncbi;
sub dynamic_cast {
    my $type = shift;
    my $obj = shift;
    my $cmd = $type . '::__dynamic_cast_to__($obj)';
    return eval($cmd);
}


############ Spec() -- find and print ASN.1 specification ##########

# Kind of cheesy right now: call Python!
package ncbi;
sub Spec {
    my $arg = shift;
    my $type;
    if ($arg->isa('HASH')) {
        $type = $arg->GetThisTypeInfo()->GetName();
    } else {
        # a string, we hope
        $type = $arg;
    }

    my $web = shift;
    if (!$web) {
        my $cmd = "python -c \'\
import sys \
sys.path.append(\"/netopt/ncbi_tools/c++-wrappers/python\") \
import find_asn_spec \
print find_asn_spec.FindSpec(\"$type\") \
\'";
        system($cmd);
    } else {
        my $asn_spec_url_base
            = 'http://graceland.ncbi.nlm.nih.gov:6224/staff/jcherry/asn_spec/';
        my $url = $asn_spec_url_base . $type . '.html';
        my $cmd = "python -c \'\
import webbrowser \
# on unix, if DISPLAY not set, remove some graphical browsers from \
# consideration (they may not return non-zero status when they fail, \
# so the user sees nothing) \
import os \
orig_tryorder = webbrowser._tryorder \
if os.name == \"posix\": \
   if not os.environ.has_key(\"DISPLAY\"): \
\
      webbrowser._tryorder = [] \
      for br in orig_tryorder: \
         if not br in [\"mozilla\", \"netscape\"]: \
            webbrowser._tryorder.append(br) \
\                
webbrowser.open_new(\"$url\") \
webbrowser._tryorder = orig_tryorder \
\'";
        system($cmd);
    }
}


############ Doxy() -- Launch doxygen documentation in web browser ##########

# lazy way: call Python
sub Doxy {
    my $arg = shift;
    my $type;
    if ($arg->isa('HASH')) {
        $type = ref($arg);
    } else {
        # a string, we hope
        $type = $arg;
    }
    $cmd = "python -c \'\
doxy_url_base = \"http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/\" \
 \
class_name = \"$type\".split(\"::\")[-1] \
mangled_class_name = class_name.replace(\"_\", \"__\") \
if class_name[0] == \"S\": \
    class_or_struct = \"struct\" \
else: \
    class_or_struct = \"class\" \
import webbrowser \
url = doxy_url_base + class_or_struct + mangled_class_name + \".html\" \
# on unix, if DISPLAY not set, remove some graphical browsers from \
# consideration (they may not return non-zero status when they fail, \
# so the user sees nothing) \
import os \
orig_tryorder = webbrowser._tryorder \
if os.name == \"posix\": \
   if not os.environ.has_key(\"DISPLAY\"): \
\
      webbrowser._tryorder = [] \
      for br in orig_tryorder: \
         if not br in [\"mozilla\", \"netscape\"]: \
            webbrowser._tryorder.append(br) \
\                
webbrowser.open_new(url) \
webbrowser._tryorder = orig_tryorder \
\'";
    system($cmd);
}


############ Lxr() -- Launch LXR documentation in web browser ##########

# lazy way: call Python
sub Lxr {
    my $arg = shift;
    my $type;
    if ($arg->isa('HASH')) {
        $type = ref($arg);
        # chop off any namespace qualifiers
        my @tmp = split(/::/, $type);
        $type = $tmp[-1]
    } else {
        # a string, we hope
        $type = $arg;
    }

    my $url = 'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/lxr/' 
        . 'ident?i=' . $type;
    my $cmd = "python -c \'\
import webbrowser \
# on unix, if DISPLAY not set, remove some graphical browsers from \
# consideration (they may not return non-zero status when they fail, \
# so the user sees nothing) \
import os \
orig_tryorder = webbrowser._tryorder \
if os.name == \"posix\": \
   if not os.environ.has_key(\"DISPLAY\"): \
\
      webbrowser._tryorder = [] \
      for br in orig_tryorder: \
         if not br in [\"mozilla\", \"netscape\"]: \
            webbrowser._tryorder.append(br) \
\                
webbrowser.open_new(\"$url\") \
webbrowser._tryorder = orig_tryorder \
\'";
    system($cmd);
}


############ DoxySearch() -- Search doxygen documentation ##########

# lazy way: call Python
sub DoxySearch {
    my $name = shift;

    my $doxy_url_base =
        'http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC/doxyhtml/';
    my $url = $doxy_url_base . 'search.php?query=' . $name;

    my $cmd = "python -c \'\
import webbrowser \
# on unix, if DISPLAY not set, remove some graphical browsers from \
# consideration (they may not return non-zero status when they fail, \
# so the user sees nothing) \
import os \
orig_tryorder = webbrowser._tryorder \
if os.name == \"posix\": \
   if not os.environ.has_key(\"DISPLAY\"): \
\
      webbrowser._tryorder = [] \
      for br in orig_tryorder: \
         if not br in [\"mozilla\", \"netscape\"]: \
            webbrowser._tryorder.append(br) \
\                
webbrowser.open_new(\"$url\") \
webbrowser._tryorder = orig_tryorder \
\'";
    system($cmd);
}


############ TreeView() -- Serial object tree view using kxmleditor ##########

package ncbi::CSerialObject;
sub TreeView {
    my $arg = shift;
    my $xml = $arg->Xml();

    # write xml to a temp file
    use Fcntl;
    use POSIX qw(tmpnam);
    do {$fname = tmpnam()}
        until sysopen(FH, $fname, O_RDWR|O_CREAT|O_EXCL);
    print FH $xml;
    close(FH);

    # launch kxmleditor with the temp file as argument
    my $cmd = "kxmleditor $fname >& /dev/null || /usr/local/kxmleditor/bin/kxmleditor $fname >& /dev/null &";
    system($cmd);
}

############ TextView() -- Serial object text view using an editor ##########

package ncbi::CSerialObject;
sub TextView {
    my $sobj = shift;
    ncbi::CTextEditor::DisplayText($sobj->Asn());
}

############ TextEdit() -- Edit serial object as ASN.1 text ##########

package ncbi::CSerialObject;
sub TextEdit {
    my $sobj = shift;
    my $s = new ncbi::string();
    ncbi::CTextEditor::EditText($sobj->Asn(), $s);
    my $type = ref($sobj);
    my $cmd = 'new ' . $type;
    my $rv = eval($cmd);
    $rv->FromAsn($s);
    return $rv;
}

############ EntrezUrl() -- url for Seq-ids or uids ##########

package ncbi;
sub EntrezUrl {

    sub GetGi {
        (my $id, my $id1cli) = @_;
        my $seq_id;
        if (ref($id) eq 'ncbi::CSeq_id') {
            $seq_id = $id;
        } else {
            # A string?
            use POSIX;
            @res = POSIX::strtol($id);
            if ($res[0] && ($res[1] == 0)) {
                # An integer or a string rep. thereof
                return $res[0];
            }
            $seq_id = new ncbi::CSeq_id($id);
        }
        return $id1cli->AskGetgi($seq_id);
    }

    my $ids = shift;
    my $db = shift;

    my $id1cli = new ncbi::CID1Client();

    my $type = ref($ids);
    if (!$type || $type eq 'ncbi::CSeq_id') {
        $ids = [$ids];
    } elsif ($type ne 'ARRAY') {
        $ids = $ids->array();
    }

    if (!defined($db)) {
        # Assume it should be 'protein' or 'nucleotide';
        # figure out which from the *first* gi
        my $hand = ncbi::CSimpleOM::GetBioseqHandle(GetGi($ids->[0], $id1cli));
        if ($hand->GetBioseqMolType() == $ncbi::CSeq_inst_Base::eMol_aa) {
            $db = 'protein';
        } else {
            $db = 'nucleotide';
        }
    }

    my $base_url = 'http://www.ncbi.nlm.nih.gov/entrez';
    my $query_start = "/query.fcgi?db=${db}&cmd=Retrieve&list_uids=";
    my $url = $base_url . $query_start;
    my $i = 0;
    foreach $id (@$ids) {
        if ($i > 0) {
            $url .= ',';
        }
        $url .= GetGi($ids->[$i], $id1cli);
        ++$i;
    }
    return $url;
}

############ EntrezWeb() -- pop up web page for Seq-ids or uids ##########

package ncbi;
sub EntrezWeb {
    my $ids = shift;
    my $db = shift;

    my $url = ncbi::EntrezUrl($ids, $db);

    my $cmd = "python -c \'\
import webbrowser \
# on unix, if DISPLAY not set, remove some graphical browsers from \
# consideration (they may not return non-zero status when they fail, \
# so the user sees nothing) \
import os \
orig_tryorder = webbrowser._tryorder \
if os.name == \"posix\": \
   if not os.environ.has_key(\"DISPLAY\"): \
\
      webbrowser._tryorder = [] \
      for br in orig_tryorder: \
         if not br in [\"mozilla\", \"netscape\"]: \
            webbrowser._tryorder.append(br) \
\                
webbrowser.open_new(\"$url\") \
webbrowser._tryorder = orig_tryorder \
\'";
    system($cmd);
}


%}

#endif


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
