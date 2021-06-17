# $Id$
import ast
from datetime import date, datetime, timedelta, timezone
from distutils.sysconfig import parse_makefile, expand_makefile_vars
import hashlib
import mmap
import os
import pwd
import re
import subprocess
import time
from warnings import warn

class IrrelevantCommandError(Exception):
    pass

class Collector(object):
    def init(self, wanted):
        self.info    = { 'tech_stack': 'cxx' }
        self.wanted  = wanted

    def in_want_list(self, key):
        if key in self.wanted or '*' in self.wanted:
            return True
        else:
            return False

    def run_command(self, command):
        start_time   = datetime.now(timezone.utc)
        status       = subprocess.call(command, close_fds = False)
        end_time     = datetime.now(timezone.utc)
        if os.fork() > 0:
            if status < 0:
                status = 128 - status
            os._exit(status) # continue in background

        self.info['start_time']   = start_time
        self.info['end_time']     = end_time
        self.info['duration']     = (end_time - start_time).total_seconds()
        self.info['succeeded']    = status == 0
        self.info['command_line'] = ' '.join(command)
        self.info['directory']    = os.getcwd()

        return status

    def collect_target_info(self, target_name, target_type, target_fullpath, srcdir, mf):
        self.info['name'] = target_name
        self.info['source_directory'] = srcdir

        if target_type == 'lib':
            self.info['type'] = 'library'
        else:
            self.info['type'] = 'app'
        if target_type == 'app' and False:
            try:
                cmd = [target_fullpath, '-version']
                self.info['app_version'] = subprocess.check_output(cmd)
            except subprocess.CalledProcessError:
                pass

        if self.in_want_list('contact'):
            self.info['contact'] = self.get_contact(mf)

    def collect_vcs_info(self):
        if 'source_directory' not in self.info:
            return

        if self.in_want_list('vcs_type'):
            vcs_info = self.get_vcs_info(self.info['source_directory'])
        else:
            vcs_info = None

        if vcs_info is not None:
            self.info.update(vcs_info)

    def collect_tc_info(self):
        tcprops = self.read_teamcity_properties()

        if 'teamcity.version' in tcprops:
            self.info['build_type'] = 'standard'
        else:
            self.info['build_type'] = 'legacy'
        if 'teamcity.build.id' in tcprops:
            self.info['build_id'] = tcprops['teamcity.build.id']
        if 'build.number' in tcprops:
            self.info['build_number'] = tcprops['build.number']

        if 'teamcity.version' in tcprops:
            self.info['tc_vars']    = tcprops
            if 'teamcity.agent.name' in tcprops:
                self.info['tc_agent_name'] = tcprops['teamcity.agent.name']

    def collect_env_info(self):
        if 'NCBI_AUTOMATED_BUILD' in os.environ:
            self.info['execution_type'] = 'automated'
        else:
            self.info['execution_type'] = 'manual'

        if self.in_want_list('env_vars'):
            self.info['env_vars']   = dict(os.environ)

    def collect_build_config(self, status_dir):
        if self.in_want_list('build_config'):
            bcfg = {}
            with open(os.path.join(status_dir, 'config.log'), 'r') as f:
                uid = os.fstat(f.fileno()).st_uid
                try:
                    bcfg['user_id'] = pwd.getpwuid(uid)[0]
                except:
                    bcfg['user_id'] = uid
                for l in f:
                    if l.startswith('  $ '):
                        bcfg['command'] = l[4:].rstrip('\n')
                    elif l.startswith('hostname = '):
                        bcfg['host'] = l[11:].rstrip('\n')
                    elif ' configurables below ' in l:
                        bcfg['cwd'] = l[l.find(' below ') + 7:].rstrip('.\n')
            self.info['build_config'] = bcfg

    def collect_artifact_info(self, sc_version, status):
        if sc_version is not None and sc_version > 0:
            self.info['artifact_version'] = 'SC-%d' % sc_version
        else:
            self.info['artifact_version'] = 'trunk'
        
        filename = self.get_target_path(self.info['name'], self.info['type'])
        self.info['artifact_name'] = os.path.basename(filename)
        if status == 0 and self.in_want_list('artifact_hash'):
            h = self.get_artifact_hash(filename)
            if h is not None:
                self.info['artifact_hash'] = h
 
    def collect(self, command, status_dir, wanted = ('*',), sc_version = None):
        try:
            command_info = self.parse_command(command)
        except IrrelevantCommandError:
            os.execv(command[0], command)

        self.init(wanted)
        status = self.run_command(command)

        target_type = command_info['target_type']
        mfname = 'Makefile.%s.%s' % (command_info['target_name'], target_type)
        srcdir = os.path.realpath(self.get_srcdir(command_info, mfname))
        mf = self.read_makefile(os.path.join(srcdir, mfname),
                                command_info['target_name'], target_type)
        target_name = expand_makefile_vars('$(%s)' % target_type.upper(), mf)
        target_fullpath = os.path.join('.', target_name)

        # order matters in some cases. Reorder these call at your own peril
        self.collect_target_info(target_name, target_type, target_fullpath, srcdir, mf)
        self.collect_vcs_info()
        self.collect_tc_info()
        self.collect_env_info()
        self.collect_build_config(status_dir)
        self.collect_artifact_info(sc_version, status)
        if self.in_want_list('libs'):
            self.info['libs'] \
                = ','.join(self.get_libs_from_log(command_info['target_name']))

    def get_as_string(self, name):
        v = self.info[name]
        if isinstance(v, str):
            return v
        elif isinstance(v, bool):
            if v:
                return 'T'
            else:
                return 'F'
        elif isinstance(v, date):
            return v.isoformat()
        else:
            return repr(v)

    def parse_command(self, command):
        if command[0].endswith('.sh'):
            raise IrrelevantCommandError

        irrelevant_re = re.compile(r'(?:check|clean|export-headers'
                                   + r'|mark-as-disabled|purge|requirements'
                                   + r'|sources)(?:[._].+)?$')
        template_re   = re.compile(r'/Makefile\.(app|lib).tmpl$')
        wrapper_re    = re.compile(r'Makefile\.(.*)_(app|lib)$')

        info                = {}
        irrelevant_targets  = []
        relevant_targets    = []
        value_expected      = False
        for x in command[1:]:
            if value_expected:
                match_info = wrapper_re.match(x)
                if match_info is not None:
                    (info['target_name'], info['target_type']) \
                        = match_info.groups()
                    info['srcdir'] = '.'
                else:
                    match_info = template_re.search(x)
                    if match_info is not None:
                        info['target_type'] = match_info.group(1)
                value_expected = False
            elif len(x) == 2 and x[0] == '-' and x[1] in 'CIWfo':
                value_expected = True
            elif irrelevant_re.match(x) is not None:
                irrelevant_targets.append(x)
            elif x.startswith('TMPL='):
                info['target_name'] = x[5:]
            elif x.startswith('srcdir='):
                info['srcdir'] = x[7:]
            elif x[0] != '-' and not '=' in x:
                relevant_targets.append(x)
        if len(info) < 3 \
           or (len(irrelevant_targets) > 0 and len(relevant_targets) == 0):
            raise IrrelevantCommandError
        return info

    def get_srcdir(self, command_info, mfname):
        if 'srcdir' in command_info:
            return command_info['srcdir']
        elif os.path.exists(mfname):
            return '.'
        elif os.path.exists('Makefile'):
            mf = parse_makefile('Makefile')
            return expand_makefile_vars('$(srcdir)', mf)
        else:
            return re.sub('/[^/]*/build/', '/src/', os.getcwd())
    
    def read_makefile(self, mfpath, target_name, target_type):
        try:
            return parse_makefile(mfpath)
        except IOError:
            return { target_type.upper(): target_name }

    def read_teamcity_properties(self):
        props = {}
        if 'TEAMCITY_BUILD_PROPERTIES_FILE' in os.environ and \
           (self.in_want_list('build_type')
            or self.in_want_list('build_number')
            or self.in_want_list('tc_agent_name')):
            fname = os.environ['TEAMCITY_BUILD_PROPERTIES_FILE']
            try:
                with open(fname, 'r') as f:
                    prop_re = re.compile(r'((?:(?![:=])\S|\\.)+)'
                                         + r'(?:\s*[:=]\s*|\s+)(.*)')
                    for l in f:
                        l = l.lstrip()
                        if len(l) == 0 or l[0] in '#!':
                            continue
                        l = l.rstrip('\n')
                        while (l.endswith('\\')):
                            l = l.rstrip('\\') + f.next().lstrip().rstrip('\n')
                        mi = prop_re.match(l)
                        if mi is None:
                            warn('Malformed line in ' + fname + ': ' + l)
                        else:
                            k = ast.literal_eval("'''"+mi.group(1)+"'''")
                            v = ast.literal_eval("'''"+mi.group(2)+"'''")
                            props[k] = v
            except Exception as e:
                warn("Failed to open %s: %s" % (fname, e))
                pass
        if len(props) == 0:
            if 'NCBI_BUILD_SESSION_ID' in os.environ:
                props['build.number'] = os.environ['NCBI_BUILD_SESSION_ID']
            # Synthesize anything else?
            pass
        return props
                                
    def get_vcs_info(self, srcdir, rest = (), fallback = None):
        if os.path.isdir(os.path.join(srcdir, '.svn')):
            return self.get_svn_info(srcdir, rest)
        elif os.path.isdir(os.path.join(srcdir, '.git')):
            return self.get_git_info(srcdir, rest)
        elif len(rest) == 0 and os.path.isdir(os.path.join(srcdir, 'CVS')):
            return self.get_cvs_info(srcdir)
        elif os.path.isfile(os.path.join(srcdir,
                                         'include/common/ncbi_package_ver.h')):
            fallback = self.get_package_info(srcdir, rest)

        if srcdir != '/':
            (d, b) = os.path.split(srcdir)
            return self.get_vcs_info(d, (b,) + rest, fallback)
        else:
            return fallback

    def get_svn_info(self, srcdir, rest):
        info = { 'vcs_type': 'svn' }
        with subprocess.Popen(['svn', 'info', os.path.join(srcdir, *rest)],
                              stdout = subprocess.PIPE,
                              stderr = subprocess.DEVNULL,
                              universal_newlines = True) as svn:
            for l in svn.stdout:
                (k, v) = l.rstrip('\n').split(': ', 1)
                if k == 'URL':
                    info['vcs_path'] = v
                    if '/trunk/' in v:
                        info['vcs_branch'] = 'trunk'
                    else:
                        match_info = re.search('/components/[^/]+/([0-9.]+)/',
                                               v)
                        if match_info is not None:
                            info['vcs_branch'] = 'SC-' + match_info.group(1)
                        else:
                            match_info = re.search('/branches/([^/]+)/', v)
                            if match_info is not None:
                                info['vcs_branch'] = match_info.group(1)
                    break
        if 'vcs_path' not in info:
            # Maybe controlled by git after all, in a hybrid layout?
            if os.path.isdir(os.path.join(srcdir, '.git')):
                return self.get_git_info(srcdir, rest)
            while srcdir != '/':
                (srcdir, child) = os.path.split(srcdir)
                if os.path.isdir(os.path.join(srcdir, '.git')):
                    return self.get_git_info(srcdir, (child,) + rest)
            return None
        return info

    def get_git_info(self, srcdir, rest):
        info = { 'vcs_type': 'git' }
        git = os.environ.get('TEAMCITY_GIT_PATH', 'git')
        url = None
        try:
            cmd = [git, 'remote', 'get-url', 'origin'] 
            url = subprocess.check_output(cmd, stderr = subprocess.DEVNULL,
                                          universal_newlines = True,
                                          cwd = srcdir)
            url = url.rstrip('\n')
        except subprocess.CalledProcessError:
            try:
                cmd = [git, 'remote', 'show', 'origin'] 
                with subprocess.Popen(cmd, stdout = subprocess.PIPE,
                                      stderr = subprocess.DEVNULL,
                                      universal_newlines = True,
                                      cwd = srcdir) as remote:
                    for l in remote.stdout:
                        (k, v) = l.strip().split(': ', 1)
                        if k == 'Fetch URL':
                            url = v
                            break
            except subprocess.CalledProcessError:
                pass
            if url is None:
                url = 'file://' + srcdir
        if url is not None:
            if len(rest) > 0:
                url = url + '#' + os.path.join(*rest)
            info['vcs_path'] = url
        try:
            cmd = [git, 'rev-parse', '--symbolic-full-name', 'HEAD']
            rev = subprocess.check_output(cmd, stderr = subprocess.DEVNULL,
                                          universal_newlines = True,
                                          cwd = srcdir)
            rev = rev.rstrip('\n')
            info['vcs_branch'] = re.sub(r'^refs/(?:heads|tags)/', '', rev)
        except subprocess.CalledProcessError:
            pass
        if 'vcs_branch' not in info and info['vcs_path'].startswith('file://'):
            # Maybe controlled by Subversion after all, in a hybrid layout?
            # (No need to check for .svn at this level, because get_svn_info
            # looks for it first.)
            while srcdir != '/':
                (srcdir, child) = os.path.split(srcdir)
                if os.path.isdir(os.path.join(srcdir, '.svn')):
                    return self.get_svn_info(srcdir, (child,) + rest)
            return None
        return info

    def get_cvs_info(self, srcdir):
        info = { 'vcs_type': 'cvs' }
        cvs_dir = os.path.join(srcdir, 'CVS')
        with open(os.path.join(cvs_dir, 'Root'), 'r') as f:
            cvs_root = f.readline().rstrip('\n')
        with open(os.path.join(cvs_dir, 'Repository'), 'r') as f:
            cvs_path = f.readline().rstrip('\n')
            if cvs_path.startswith('/'):
                pos = cvs_root.find(':') + 1
                info['vcs_path'] = cvs_root[:pos] + cvs_path
            else:
                info['vcs_path'] = cvs_root + '/' + cvs_path
        with open(os.path.join(cvs_dir, 'Entries'), 'r') as f:
            l = f.readline().rstrip('\n')
            match_info = re.match(r'/.*?/.*?/.*?/.*?/[^D](.+)', l)
            if match_info is None:
                info['vcs_branch'] = 'HEAD'
            else:
                info['vcs_branch'] = match_info.group(1)
        return info

    def get_package_info(self, srcdir, rest):
        filename = os.path.join(srcdir, 'include/common/ncbi_package_ver.h')
        package_name = None
        version      = [None, None, None]
        with open(filename) as f:
            for l in f:
                if l.startswith('#define NCBI_PACKAGE_'):
                    words = l.split()
                    if words[1] == 'NCBI_PACKAGE_NAME':
                        package_name = words[2].strip('"')
                    elif words[1] == 'NCBI_PACKAGE_VERSION_MAJOR':
                        version[0] = words[2]
                    elif words[1] == 'NCBI_PACKAGE_VERSION_MINOR':
                        version[1] = words[2]
                    elif words[1] == 'NCBI_PACKAGE_VERSION_PATCH':
                        version[2] = words[2]
            if package_name is not None and version[0] is not None \
               and version[1] is not None and version[2] is not None \
               and (package_name != 'unknown' or version != ['0', '0', '0']):
                base    = 'https://svn.ncbi.nlm.nih.gov/repos/toolkit/release'
                version = '.'.join(version)
                url     = '/'.join((base, package_name, version, 'c++') + rest)
                return { 'vcs_type':   'svn',
                         'vcs_path':   url,
                         'vcs_branch': package_name + '-' + version }
        return None
    
    def get_contact(self, mf):
        next_dir = os.getcwd()
        while mf is not None:
            if 'WATCHERS' in mf:
                return expand_makefile_vars('$(WATCHERS)', mf)
            elif next_dir is None:
                break
            mfname = os.path.join(next_dir, 'Makefile')
            if os.path.exists(mfname):
                mf = parse_makefile(mfname)
            else:
                break
            if next_dir == '/':
                next_dir = None
            else:
                next_dir = os.path.dirname(next_dir)

        return '-'
        # if 'LOGNAME' in os.environ:
        #     return os.environ['LOGNAME']
        # elif 'USER' in os.environ:
        #     return os.environ['USER']
        # else:
        #     uid = os.getuid()
        #     try:
        #         return pwd.getpwuid(uid)[0]
        #     except:
        #         return str(uid)

    def get_target_path(self, target_name, target_type):
        if target_type == 'app':
            if os.path.exists(target_name + '.exe'):
                filename = target_name + '.exe'
            else:
                filename = target_name
        else:
            filename = 'lib' + target_name
            for x in ('.dylib', '-dll.dylib', '.so', '-dll.so', '.a'):
                if os.path.exists(filename + x):
                    filename = filename + x
                    break

        return filename
 
    def get_artifact_hash(self, filename):
        if not os.path.exists(filename):
            warn('Unable to find ' + filename + ' to hash')
            return None
        with open(filename, 'rb') as f:
            with mmap.mmap(f.fileno(), 0, access = mmap.ACCESS_READ) as mm:
                return hashlib.md5(mm).hexdigest()

    def get_libs_from_log(self, project_name):
        filename = 'make_' + project_name + '.log'
        if not os.path.exists(filename):
            warn('Unable to find ' + filename + ' to examine')
            # Fall back on readelf -d (or otool, on macOS)?
            return set()
        last_link_line = ''
        with open(filename, 'r', errors='ignore') as f:
            for l in f:
                if l.find(' -l') >= 0:
                    last_link_line = l
        return self.get_libs_from_command(last_link_line.split())

    def get_libs_from_command(self, command):
        libs = set()
        skip = False
        for x in command:
            if skip:
                skip = False
                continue
            elif x.startswith('-'):
                if x.startswith('-l'):
                    l = x[2:]
                elif x == '-o':
                    skip = True
                    continue
                else:
                    continue
            elif x.endswith('.a') or x.endswith('.so') or x.endswith('.dylib'):
                l = x[x.rfind('/')+1:x.rfind('.')]
                if l.startswith('lib'):
                    l = l[3:]
            else:
                continue
            if l.endswith('-dll'):
                l = l[:-4]
            elif l.endswith('-static'):
                l = l[:-7]
            libs.add(l)
        return libs


class CollectorCMake(Collector):
    def collect(self, command, top_src_dir, wanted = ('*',), sc_version = None):
        try:
            command_info = self.parse_command(command)
        except IrrelevantCommandError:
            os.execv(command[0], command)

        self.init(wanted)
        status = self.run_command(command)

        target_type = command_info['target_type']
        target_name = command_info['target_name']
        self.target_fullpath = command_info['target_fullpath']
        path = os.getcwd()
        src_dir = re.sub('/[^/]*/build/', '/src/', path) # tentatively
        tail = ''
        while path != '/':
            cache_name = os.path.join(path, 'CMakeCache.txt')
            if os.path.exists(cache_name):
                break
            (path, child) = os.path.split(path)
            tail = os.path.join(child, tail)
        if os.path.exists(cache_name):
            with open(cache_name, 'r', errors='ignore') as f:
                src_dir_re = re.compile('^CPP_SOURCE_DIR:.+=(.+)')
                for l in f:
                    match_info = src_dir_re.match(l)
                    if match_info is not None:
                        src_dir = os.path.join(match_info.group(1),
                                               tail.rstrip('/'))

        # order matters in some cases. Reorder these call at your own peril
        self.collect_target_info(target_name, target_type,
                                 self.target_fullpath, src_dir, None)
        self.collect_vcs_info()
        self.collect_tc_info()
        self.collect_env_info()
        self.collect_artifact_info(sc_version, status)
        if self.in_want_list('libs'):
            self.info['libs'] = ','.join(self.get_libs_from_command(command))

    def get_target_path(self, target_name, target_type):
        return self.target_fullpath

    def parse_command(self, command):
        if not command[0].endswith('g++') and not command[0].endswith('gcc'):
            raise IrrelevantCommandError

        info                = {}
        value_expected      = False
        for x in command[1:]:
            if value_expected:
                output_path = x
                info['target_fullpath'] = os.path.abspath(output_path)
                target_filename = os.path.basename(output_path)
                (target_name,ext) = os.path.splitext(target_filename)
                info['target_name'] = target_name
                if ext in ('.so', '.a', '.lib', '.dll', '.dylib'):
                    info['target_type'] = 'lib'
                    if info['target_name'].startswith("lib"):
                        info['target_name'] = info['target_name'][3:]
                elif ext in ('.o', '.obj'):
                    raise IrrelevantCommandError
                else:
                    if ext and ext != '.exe':
                        info['target_name'] = target_filename
                    info['target_type'] = 'app'
                value_expected = False
            elif x == '-o':
                value_expected = True

        if len(info) != 3:
            raise IrrelevantCommandError

        return info


