# $Id$
import ast
from datetime import date, datetime, timedelta, timezone
from distutils.sysconfig import parse_makefile, expand_makefile_vars
import os
import pwd
import re
import subprocess
import time
from warnings import warn

class IrrelevantCommandError(Exception):
    pass

class Collector(object):
    def collect(self, command, status_dir, wanted = ('*',), sc_version = None):
        try:
            command_info = self.parse_command(command)
        except IrrelevantCommandError:
            os.execv(command[0], command)

        self.info    = { }
        start_time   = datetime.now(timezone.utc)
        status       = subprocess.call(command, close_fds = False)
        end_time     = datetime.now(timezone.utc)
        if os.fork() > 0:
            if status < 0:
                status = 128 - status
            os._exit(status) # continue in background

        target_type = command_info['target_type']
        mfname = 'Makefile.%s.%s' % (command_info['target_name'], target_type)
        srcdir = os.path.realpath(self.get_srcdir(command_info, mfname))
        mf = self.read_makefile(os.path.join(srcdir, mfname),
                                command_info['target_name'], target_type)
        target_name = expand_makefile_vars('$(%s)' % target_type.upper(), mf)
        tcprops = self.read_teamcity_properties(wanted)
        if 'vcs_type' in wanted or '*' in wanted:
            vcs_info = self.get_vcs_info(srcdir)
        else:
            vcs_info = None

        self.info['name'] = target_name
        if target_type == 'lib':
            self.info['type'] = 'library'
        else:
            self.info['type'] = 'app'
        if target_type == 'app' and False:
            try:
                cmd = [os.path.join('.', target_name), '-version']
                self.info['app_version'] = subprocess.check_output(cmd)
            except subprocess.CalledProcessError:
                pass
        # contact -- tentative, needs clarification
        if 'LOGNAME' in os.environ:
            self.info['contact'] = os.environ['LOGNAME']
        elif 'USER' in os.environ:
            self.info['contact'] = os.environ['USER']
        else:
            uid = os.getuid()
            try:
                self.info['contact'] = pwd.getpwuid(uid)[0]
            except:
                self.info['contact'] = str(uid)
        self.info['start_time'] = start_time
        self.info['end_time']   = end_time
        self.info['duration']   = (end_time - start_time).total_seconds()
        self.info['succeeded']  = status == 0
        if 'teamcity.version' in tcprops:
            self.info['build_type'] = 'standard'
        else:
            self.info['build_type'] = 'legacy'
        if 'NCBI_AUTOMATED_BUILD' in os.environ:
            self.info['execution_type'] = 'automated'
        else:
            self.info['execution_type'] = 'manual'
        self.info['directory'] = os.getcwd()
        self.info['source_directory'] = srcdir
        if 'teamcity.build.id' in tcprops:
            self.info['build_id'] = tcprops['teamcity.build.id']
        if 'build.number' in tcprops:
            self.info['build_number'] = tcprops['build.number']
        if vcs_info is not None:
            self.info.update(vcs_info)
        self.info['artifact_name'] = target_name
        if sc_version is not None and sc_version > 0:
            self.info['artifact_version'] = 'SC-%d' % sc_version
        else:
            self.info['artifact_version'] = 'trunk'
        self.info['command_line'] = ' '.join(command)
        # deployment-regions -- needs clarification
        # devops_step_name -- ???
        if 'env_vars' in wanted or '*' in wanted:
            self.info['env_vars']   = dict(os.environ)
        if 'teamcity.version' in tcprops:
            self.info['tc_vars']    = tcprops
            if 'teamcity.agent.name' in tcprops:
                self.info['tc_agent_name'] = tcprops['teamcity.agent.name']
        elif 'build_config' in wanted or '*' in wanted:
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

    def read_teamcity_properties(self, wanted):
        props = {}
        if 'TEAMCITY_BUILD_PROPERTIES_FILE' in os.environ and \
           ('build_type' in wanted or 'build_id' in wanted
            or 'build_number' in wanted or 'tc_vars' in wanted
            or 'tc_agent_name' in wanted or '*' in wanted):
            fname = os.environ['TEAMCITY_BUILD_PROPERTIES_FILE']
            try:
                with open(fname, 'r') as f:
                    prop_re \
                        = re.compile(r'((?:\S*|\\\s)+)(?:\s*[:=]\s*|\s+)(.*)')
                    for l in f:
                        l = l.lstrip()
                        if len(l) == 0 or l[0] in '#!':
                            next
                        l = l.rstrip('\n')
                        while (l.endswith('\\')):
                            l = l + f.next().lstrip().rstrip('\n')
                            mi = prop_re.match(l)
                            if mi is None:
                                warn('Malformed line in ' + fname + ': ' + l)
                            else:
                                k = ast.literal_eval("'''"+mi.group(1)+"'''")
                                v = ast.literal_eval("'''"+mi.group(2)+"'''")
                                props[k] = v
            except:
                pass
        if len(props) == 0:
            if 'NCBI_BUILD_SESSION_ID' in os.environ:
                props['build.number'] = os.environ['NCBI_BUILD_SESSION_ID']
            # Synthesize anything else?
            pass
        return props
                                
    def get_vcs_info(self, srcdir, rest = ()):
        if os.path.isdir(os.path.join(srcdir, '.svn')):
            return self.get_svn_info(srcdir, rest)
        elif os.path.isdir(os.path.join(srcdir, '.git')):
            return self.get_git_info(srcdir, rest)
        elif len(rest) == 0 and os.path.isdir(os.path.join(srcdir, 'CVS')):
            return self.get_cvs_info(srcdir)
        elif srcdir != '/':
            (d, b) = os.path.split(srcdir)
            return self.get_vcs_info(d, (b,) + rest)
        else:
            return None

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
        try:
            cmd = ['git', '-C', srcdir, 'remote', 'get-url', 'origin'] 
            url = subprocess.check_output(cmd, stderr = subprocess.DEVNULL,
                                          universal_newlines = True)
            url = url.rstrip('\n')
            if len(rest) > 0:
                url = url + '#' + os.path.join(*rest)
            info['vcs_path'] = url
        except subprocess.CalledProcessError:
            info['vcs_path'] = 'file://' + os.path.join(srcdir, *rest)
        try:
            cmd = ['git', '-C', srcdir, 'rev-parse', '--symbolic-full-name',
                   'HEAD']
            rev = subprocess.check_output(cmd, stderr = subprocess.DEVNULL,
                                          universal_newlines = True)
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
            cvs_root = f.getline().rstrip('\n')
        with open(os.path.join(cvs_dir, 'Repository'), 'r') as f:
            cvs_path = f.getline().rstrip('\n')
            if cvs_path.startswith('/'):
                pos = cvs_root.find(':') + 1
                info['vcs_path'] = cvs_root[:pos] + cvs_path
            else:
                info['vcs_path'] = cvs_root + '/' + cvs_path
        with open(os.path.join(cvs_dir, 'Entries'), 'r') as f:
            l = f.getline().rstrip('\n')
            match_info = re.match(r'/.*?/.*?/.*?/.*?/[^D](.+)', l)
            if match_info is None:
                info['vcs_branch'] = 'HEAD'
            else:
                info['vcs_branch'] = match_info.group(1)
        return info
