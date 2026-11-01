#!/usr/bin/env python3
"""The native release archives of an engine repository, made the same way by its CI and by a local release.

Every engine repository vendors this file as vendor/release/package.py. Its source is the frontend's
packages/build/engine-release/package.py, and packages/build/bin/vendor-release.mjs writes and checks the copies:
re-vendor, never edit a copy. What differs per game is the repository's release/release.json (the frontend's
packages/build/README.md, "The release packer").

    package.py version
    package.py game
    package.py archives
    package.py check
    package.py list <archive>
    package.py archive <archive> --version <v> --out <dir> [--kind test|release] [--absent <extra>]...
    package.py notes --version <v> [--kind test|release] [--absent <extra>]... [--glibc-from <file>...]
    package.py copy-release <extra> --version <v> --out <dir> [--github-repo <owner/name>]

  version       the checkout's version: X.Y.Z at a release tag (vX.Y.Z, or vX.Y.Z-<a variant's tag suffix>), else
                `git describe` from the newest release tag, e.g. 1.2.0-14-g54485b0. A shallow clone is refused.
  game          the game this checkout builds (release.json picks it by the variant, when it has variants)
  archives      the game's archives: its desktop platforms, then its extra archives
  check         release.json is complete for every game it lists, and every repository file it names exists
  list          the files an archive takes, "<path in the archive>  <source>" per line, without writing anything
  archive       writes <out>/<game>-<version>-<archive>: .tar.gz for Linux, .zip for Windows and the extras, .apk
                for android (the release APK Gradle built)
  notes         prints the release notes of every desktop platform (Markdown that reads as plain text)
  copy-release  writes an extra archive from the repository's newest versioned GitHub release (`gh`) when the
                files there match the extra's sha256 list; exit 3 when they are absent or differ

<archive>: linux-x86, linux-x86_64, windows-x86, windows-x86_64, android, or one of release.json's extras.

  --repo <dir>        the engine checkout (default: the repository this file is vendored into, two folders up)
  --build-root <dir>  the folder holding the build trees release.json names (default: the checkout)
  --kind test         a build of the repository's CI: the APK is <game>-<version>-android-test.apk and the notes say
                      it is a test build (the default); --kind release names our own releases' -android-release.apk
  --absent <extra>    an extra archive this build does not have: the notes say so instead of describing it
  --glibc-from <file>...  Linux binaries or .tar.gz archives the notes measure the glibc version from: the newest
                      any of them needs (default: the Linux build trees)
"""
import argparse
import glob
import gzip
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import time
import zipfile

DESKTOP = ('linux-x86', 'linux-x86_64', 'windows-x86', 'windows-x86_64')
EXIT_ABSENT = 3
WIDTH = 115
# The oldest release of each distribution that has at least this glibc.
UBUNTU = (((2, 31), 'Ubuntu 20.04'), ((2, 35), 'Ubuntu 22.04'), ((2, 39), 'Ubuntu 24.04'), ((2, 43), 'Ubuntu 26.04'))
DEBIAN = (((2, 31), 'Debian 11'), ((2, 36), 'Debian 12'), ((2, 41), 'Debian 13'))
GLIBC_RE = re.compile(rb'GLIBC_(\d+)\.(\d+)(?:\.(\d+))?')


class Fail(Exception):
    pass


def fail(msg):
    raise Fail(msg)


# ---- release.json ------------------------------------------------------------------------------------------------

def load(repo):
    path = os.path.join(repo, 'release', 'release.json')
    if not os.path.isfile(path):
        fail('%s is missing: the game data of this repository' % path)
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def variant_of(repo, data):
    sel = data.get('variant')
    if not sel:
        return None
    path = os.path.join(repo, sel['file'])
    with open(path, encoding='latin-1') as f:
        hits = re.findall(sel['pattern'], f.read(), re.M)
    if not hits:
        fail('%s: no match for the variant pattern %s' % (path, sel['pattern']))
    return hits[-1] if sel.get('match') == 'last' else hits[0]


def games(data):
    """Every game of release.json, each with the shared keys ("common") under its own."""
    out = []
    for g in data['games']:
        merged = dict(data.get('common', {}))
        merged.update(g)
        out.append(merged)
    return out


def game_of(repo, data):
    variant = variant_of(repo, data)
    pick = [g for g in games(data) if variant is None or g.get('variant') == variant]
    if len(pick) != 1:
        fail('release.json: %d games for variant %r' % (len(pick), variant))
    return pick[0]


def extras_of(g):
    return [e['name'] for e in g.get('extras', [])]


def extra(g, name):
    for e in g.get('extras', []):
        if e['name'] == name:
            return e
    fail('%s has no archive %r (%s)' % (g['id'], name, ', '.join(list(DESKTOP) + ['android'] + extras_of(g))))


# ---- version -----------------------------------------------------------------------------------------------------

def git(repo, *args):
    r = subprocess.run(['git', '-C', repo] + list(args), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True)
    return r.returncode, r.stdout.strip()


def version_of(repo, data):
    rc, shallow = git(repo, 'rev-parse', '--is-shallow-repository')
    if rc != 0:
        fail('%s is not a git checkout' % repo)
    if shallow == 'true':
        fail('a shallow clone has no tags to describe: fetch the whole history (actions/checkout: fetch-depth: 0)')
    suffixes = data.get('tagSuffixes', [])

    def plain(tag):
        v = tag[1:]
        for s in suffixes:
            v = re.sub(r'-%s(?=$|-)' % re.escape(s), '', v)
        return v

    _, here = git(repo, 'tag', '--points-at', 'HEAD', '--list', 'v[0-9]*')
    tags = sorted(here.split(), key=lambda t: ('-rc' in t, t))
    if tags:
        return plain(tags[0])
    rc, desc = git(repo, 'describe', '--tags', '--match', 'v[0-9]*', '--exclude', '*-rc*', '--abbrev=7')
    if rc == 0 and desc:
        return plain(desc)
    _, short = git(repo, 'rev-parse', '--short=7', 'HEAD')
    return '0.0.0-0-g' + short


# ---- the files of an archive -------------------------------------------------------------------------------------

def desktop_files(repo, build_root, g, platform):
    p = g['platforms'].get(platform)
    if not p:
        fail('%s has no platform %s' % (g['id'], platform))
    b = os.path.join(build_root, p['build'])
    if not os.path.isdir(b):
        fail('%s %s: %s is not built' % (g['id'], platform, b))
    files = [('System/' + os.path.basename(rel), os.path.join(b, rel))
             for rel in [p['client']] + ([p['ucc']] if p.get('ucc') else []) + p.get('files', [])]
    files += [('System/' + os.path.basename(rel), os.path.join(repo, rel)) for rel in g.get('inis', [])]
    return files


def roots_of(repo, build_root, e):
    roots = {'repo': repo}
    if e.get('build'):
        roots['build'] = os.path.join(build_root, e['build'])
    for name, spec in e.get('roots', {}).items():
        roots[name] = os.environ.get(spec['env']) or os.path.normpath(os.path.join(repo, spec['path']))
    return roots


def extra_files(repo, build_root, e):
    roots = roots_of(repo, build_root, e)
    files = []
    for spec in e.get('files', []):
        hits = sorted(glob.glob(os.path.join(roots[spec['from']], spec['glob'])))
        if not hits:
            fail('%s: nothing matches %s' % (e['name'], os.path.join(roots[spec['from']], spec['glob'])))
        files += [(spec['to'] + os.path.basename(h), h) for h in hits]
    if e.get('list'):
        with open(os.path.join(repo, e['list']), encoding='utf-8') as f:
            for line in f:
                parts = line.split()
                if not parts or parts[0].startswith('#') or parts[0] not in e['listSources']:
                    continue
                files.append((parts[2], os.path.join(roots[parts[0]], parts[1])))
    return files


def check_present(what, files):
    missing = [src for _, src in files if not os.path.isfile(src)]
    if missing:
        fail('%s: missing %s' % (what, ', '.join(missing)))
    names = [a for a, _ in files]
    twice = sorted(set(a for a in names if names.count(a) > 1))
    if twice:
        fail('%s: two files for %s' % (what, ', '.join(twice)))


def read_sums(path):
    sums = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            s = line.strip()
            if s and not s.startswith('#'):
                digest, name = s.split(None, 1)
                sums[name.lstrip('*')] = digest.lower()
    return sums


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def sum_mismatches(files, sums):
    """What differs between an archive's files and a sha256 list naming them relative to System/."""
    have = {(a[len('System/'):] if a.startswith('System/') else a): sha256(src) for a, src in files}
    bad = ['%s is missing' % n for n in sorted(sums) if n not in have]
    bad += ['%s differs' % n for n in sorted(sums) if n in have and have[n] != sums[n]]
    bad += ['%s is not in the list' % n for n in sorted(have) if n not in sums]
    return bad


# ---- writing -----------------------------------------------------------------------------------------------------

def is_program(path):
    with open(path, 'rb') as f:
        head = f.read(4)
    return head == b'\x7fELF' or head[:2] == b'MZ'


def write_archive(dest, files, readme=None):
    """A .tar.gz or .zip of `files` ([(path in the archive, source)]) and README.txt, its folders included."""
    stamp = int(os.environ.get('SOURCE_DATE_EPOCH') or time.time())
    files = sorted(files)
    dirs = sorted(set('/'.join(a.split('/')[:i]) for a, _ in files for i in range(1, a.count('/') + 1)))
    part = dest + '.part'
    if dest.endswith('.tar.gz'):
        def info(name, mode, size=0, kind=tarfile.REGTYPE):
            ti = tarfile.TarInfo(name)
            ti.mtime, ti.mode, ti.size, ti.type = stamp, mode, size, kind
            ti.uid = ti.gid = 0
            ti.uname = ti.gname = 'root'
            return ti
        with open(part, 'wb') as raw, gzip.GzipFile('', 'wb', 6, raw, stamp) as gz, \
                tarfile.open(fileobj=gz, mode='w', format=tarfile.GNU_FORMAT) as tar:
            for d in dirs:
                tar.addfile(info(d, 0o755, kind=tarfile.DIRTYPE))
            for a, src in files:
                with open(src, 'rb') as f:
                    tar.addfile(info(a, 0o755 if is_program(src) else 0o644, os.path.getsize(src)), f)
            if readme is not None:
                data = readme.encode('utf-8')
                tar.addfile(info('README.txt', 0o644, len(data)), io.BytesIO(data))
    else:
        when = time.localtime(stamp)[:6]

        def info(name, mode):
            zi = zipfile.ZipInfo(name, when)
            zi.external_attr = mode << 16
            zi.compress_type = zipfile.ZIP_DEFLATED
            return zi
        with zipfile.ZipFile(part, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            for d in dirs:
                zi = info(d + '/', 0o40755)
                zi.external_attr |= 0x10
                zi.compress_type = zipfile.ZIP_STORED
                z.writestr(zi, b'')
            for a, src in files:
                with open(src, 'rb') as f, z.open(info(a, 0o100000 | (0o755 if is_program(src) else 0o644)),
                                                  'w') as w:
                    shutil.copyfileobj(f, w, 1 << 20)
            if readme is not None:
                z.writestr(info('README.txt', 0o100644), readme.encode('utf-8'))
    os.replace(part, dest)


# ---- glibc -------------------------------------------------------------------------------------------------------

def glibc_of_bytes(data):
    if data[:4] != b'\x7fELF':
        return None
    found = [tuple(int(x) for x in m.groups() if x is not None) for m in GLIBC_RE.finditer(data)]
    return max(found) if found else None


def glibc_of(paths):
    """The newest GLIBC_x.y symbol version the ELF files (or the files of .tar.gz archives) need, or None."""
    need = []
    for p in paths:
        if p.endswith('.tar.gz'):
            with tarfile.open(p) as tar:
                for m in tar.getmembers():
                    if m.isfile():
                        need.append(glibc_of_bytes(tar.extractfile(m).read()))
        elif os.path.isfile(p):
            with open(p, 'rb') as f:
                need.append(glibc_of_bytes(f.read()))
    need = [n for n in need if n]
    return max(need) if need else None


def glibc_text(v):
    if not v:
        fail('no Linux binary to measure the glibc version from')
    first = [next((name for ver, name in table if ver >= v[:2]), None) for table in (UBUNTU, DEBIAN)]
    where = ' or '.join(n for n in first if n)
    return 'glibc %s or newer (%sa current Arch or Fedora)' % ('.'.join(map(str, v)), where + ', or ' if where else '')


# ---- the notes ---------------------------------------------------------------------------------------------------

def wrap(text, indent=''):
    return textwrap.wrap(text, WIDTH, initial_indent=indent, subsequent_indent=indent, break_on_hyphens=False)


def and_list(names):
    return names[0] if len(names) == 1 else ', '.join(names[:-1]) + ' and ' + names[-1]


def archive_name(g, version, name):
    return '%s-%s-%s%s' % (g['id'], version, name, '.tar.gz' if name.startswith('linux-') else '.zip')


def apk_name(g, version, kind):
    return '%s-%s-android-%s.apk' % (g['id'], version, kind)


def fill(g, version, line):
    line = line.replace('{version}', version).replace('{game}', g['id'])
    return re.sub(r'\{archive:([a-z0-9_-]+)\}', lambda m: archive_name(g, version, m.group(1)), line)


def render(g, version, kind, platforms, present, glibc):
    """The notes for `platforms`: every desktop platform (the release text) or one (an archive's README.txt).
    `present`: the extra archives this build has; `glibc`: the version the Linux binaries need."""
    P = g['platforms']
    exe = lambda p: os.path.basename(P[p]['client'])
    ucc = lambda p: os.path.basename(P[p]['ucc']) if P[p].get('ucc') else None
    title = g['title']
    L = ['# %s %s: native ports' % (title, version), '',
         "%s's engine rebuilt for today's systems: Linux and Windows, 32-bit and 64-bit, and Android on" % title,
         'ARM (32-bit and 64-bit) and x86_64. It contains no game data: you need your own copy of the game.', '']
    if kind == 'test':
        L += wrap("This is a test build from the repository's CI, not a release: the releases are the repository's "
                  'versioned GitHub releases.') + ['']
    libs, pkgs = g['linuxLibraries'], g['linuxPackages32']
    apt = 'sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install ' + pkgs
    if len(platforms) > 1:
        L += ['## Windows and Linux', '',
              '1. Install the game: %s.' % g['gameFiles'],
              '2. Extract the archive for your system into the game folder, the one that holds System. Its System '
              'folder',
              '   replaces files of the same name there: keep a copy of the originals if you want to go back.',
              '3. Start %s on Windows or ./%s on Linux, from the System folder.' % (exe('windows-x86_64'),
                                                                                  exe('linux-x86_64'))]
        if (exe('windows-x86'), exe('linux-x86')) != (exe('windows-x86_64'), exe('linux-x86_64')):
            L.append('   The 32-bit archives start %s and ./%s instead.' % (exe('windows-x86'), exe('linux-x86')))
        L += ['', 'Pick the archive that matches your system: x86_64 for 64-bit Windows or Linux, x86 for 32-bit.',
              '', 'Linux needs current system libraries: %s,' % glibc_text(glibc),
              'with %s. The 32-bit (x86) archive needs their 32-bit versions; on Ubuntu or Debian:' % libs, apt, '']
    else:
        p = platforms[0]
        system, arch = p.split('-', 1)
        other = 'x86_64' if arch == 'x86' else 'x86'
        bits = {'x86': '32-bit', 'x86_64': '64-bit'}
        L += ['## %s' % system.capitalize(), '',
              '1. Install the game: %s.' % g['gameFiles'],
              '2. Extract this archive into the game folder, the one that holds System. Its System folder replaces',
              '   files of the same name there: keep a copy of the originals if you want to go back.',
              '3. Start %s%s from the System folder.' % ('./' if system == 'linux' else '', exe(p)), '']
        L += wrap('This is the %s (%s) build; the %s archive is the %s one.' % (bits[arch], arch, other, bits[other]))
        if system == 'linux' and arch == 'x86':
            L += wrap('It needs current system libraries in their 32-bit versions: %s, with %s. On Ubuntu or Debian:'
                      % (glibc_text(glibc), libs)) + [apt]
        elif system == 'linux':
            L += wrap('It needs current system libraries: %s, with %s.' % (glibc_text(glibc), libs))
        L.append('')

    systems = sorted(set(p.split('-')[0] for p in platforms))
    both = len(systems) == 2
    uccs = {s: ucc(s + '-x86_64') for s in systems}
    server = g.get('server')
    if any(uccs.values()):
        run = {'linux': './%s' % uccs.get('linux'), 'windows': uccs.get('windows')}
        first = 'linux' if both else systems[0]
        L += ['## The command-line tool (UCC)', '']
        if both:
            L.append("System also holds %s (Linux) and %s (Windows), the engine's UCC:" % (uccs['linux'],
                                                                                         uccs['windows']))
        else:
            L.append("System also holds %s, the engine's UCC:" % uccs[first])
        L.append('%s (%s help lists them). It opens no window and needs no sound.'
                 % ('the dedicated server and the commandlets' if server else 'the commandlets', run[first]))
        if server:
            cmds = [('%s server "%s"' % (run[s], server), s) for s in systems]
            width = max(len(c) for c, _ in cmds)
            L += ['', 'A dedicated server, started from the System folder (add -port=<n> for another port than 7777):',
                  '']
            L += [('    %s  (%s)' % (c.ljust(width), s.capitalize())) if both else '    ' + c for c, s in cmds]
            stop = {'linux': 'Ctrl-C or SIGTERM stops it cleanly.', 'windows': 'Ctrl-C stops it cleanly.'}
            L += ['', 'Ctrl-C, or SIGTERM on Linux, stops it cleanly.' if both else stop[first]]
        else:
            L.append('There is no dedicated server: the game is single-player.')
        L.append('')

    inis = [os.path.basename(i) for i in g['inis']]
    live = g['liveInis']
    own = g.get('uccInis', [])
    many = len(live) > 1
    if own:
        makes = ('The game makes its settings %s, %s, from %s when %s missing (the UCC makes its own, %s, the same way)'
                 % ('files' if many else 'file', and_list(live), 'them' if len(inis) > 1 else 'it',
                    'they are' if many else 'it is', and_list(own)))
    else:
        makes = ('%s settings %s, %s, from %s when %s missing'
                 % ('The game and its UCC make their' if any(uccs.values()) else 'The game makes its',
                    'files' if many else 'file', and_list(live), 'them' if len(inis) > 1 else 'it',
                    'they are' if many else 'it is'))
    L += ['## Settings', '']
    L += wrap("System also holds %s, the port's default settings. %s, so the settings you already have are kept. To "
              "start from the port's defaults, delete %s in System (keep a copy to go back)."
              % (and_list(inis), makes, and_list(live + own)))
    for note in g.get('settingsNotes', []):
        L += [''] + wrap(fill(g, version, note))
    L.append('')

    for s in g.get('sections', []):
        if s.get('platforms') and not set(s['platforms']) & set(platforms):
            continue
        if not all(n in present for n in s.get('when', [])):
            continue
        if s.get('unless') and all(n in present for n in s['unless']):
            continue
        L += [fill(g, version, line) for line in s['lines']] + ['']

    folder = g['androidFolder']
    L += ['## Android', '',
          'Android %s or newer, on a phone, tablet or Chromebook (32-bit or 64-bit ARM, or x86_64).' % g['androidVersion'],
          '',
          '1. Install %s; allow installs from this source when Android asks.' % apk_name(g, version, kind),
          '2. Copy your game folder onto the device as %s, at the top of its storage or an SD card.' % folder,
          '3. Open the app, tap "Choose the %s folder" and pick it. The app plays it where it is (it asks for' % folder,
          '   All files access) or copies it into its own storage, which is faster and is removed with the app.', '']
    if kind == 'release':
        L += wrap('Later versions install over this one and keep your settings. A test build of the app (an '
                  "-android-test.apk, as the repository's CI makes) is signed with another key: it cannot update "
                  'this app, nor this app it, so uninstall the one you have first. To keep its settings and saves, '
                  'use its "Copy settings and saves" button (where it has one) before you uninstall it.')
    else:
        L += wrap('This test build is signed with a key made for this build alone: it cannot update an installed '
                  'release of the app (an -android-release.apk) or another test build, and neither can update it, '
                  'so uninstall the one you have first. To keep its settings and saves, use its "Copy settings and '
                  'saves" button (where it has one) before you uninstall it.')
    return '\n'.join(L) + '\n'


# ---- commands ----------------------------------------------------------------------------------------------------

def present_of(g, absent):
    for n in absent:
        extra(g, n)
    return [n for n in extras_of(g) if n not in absent]


def cmd_check(repo, data):
    problems = []
    need = ('id', 'title', 'gameFiles', 'androidFolder', 'androidVersion', 'linuxLibraries', 'linuxPackages32',
            'inis', 'liveInis', 'platforms', 'apk')
    for g in games(data):
        problems += ['%s: no "%s"' % (g.get('id'), k) for k in need if k not in g]
        for p in DESKTOP:
            spec = g.get('platforms', {}).get(p)
            if not spec or 'build' not in spec or 'client' not in spec:
                problems.append('%s: platform %s needs "build" and "client"' % (g.get('id'), p))
        for rel in g.get('inis', []) + [e[k] for e in g.get('extras', []) for k in ('list', 'sha256') if k in e]:
            if not os.path.isfile(os.path.join(repo, rel)):
                problems.append('%s: %s does not exist' % (g.get('id'), rel))
        names = extras_of(g)
        for s in g.get('sections', []):
            problems += ['%s: a section names the unknown archive %s' % (g['id'], n)
                         for n in s.get('when', []) + s.get('unless', []) if n not in names]
        for e in g.get('extras', []):
            kinds = [k for k in ('files', 'list', 'command') if k in e]
            if len(kinds) != 1:
                problems.append('%s: extra %s needs one of files, list, command' % (g['id'], e['name']))
            if 'fromRelease' in e and 'sha256' not in e:
                problems.append('%s: extra %s copies from a release without a sha256 list' % (g['id'], e['name']))
    g = game_of(repo, data)
    for p in problems:
        print('✗ ' + p)
    if problems:
        return 1
    print('release.json: %s (this checkout), %s' % (g['id'], ', '.join(x['id'] for x in games(data))))
    return 0


def files_of(repo, build_root, g, name):
    if name in DESKTOP:
        return desktop_files(repo, build_root, g, name)
    e = extra(g, name)
    if 'command' in e:
        fail('%s is made by a command: %s' % (name, ' '.join(e['command'])))
    return extra_files(repo, build_root, e)


def cmd_archive(repo, build_root, g, name, version, out, kind, absent):
    os.makedirs(out, exist_ok=True)
    if name == 'android':
        apks = sorted(glob.glob(os.path.join(build_root, g['apk'], '*.apk')))
        if len(apks) != 1:
            fail('android: %d APKs in %s (Gradle assembleRelease makes one)' % (len(apks),
                                                                               os.path.join(build_root, g['apk'])))
        dest = os.path.join(out, apk_name(g, version, kind))
        shutil.copyfile(apks[0], dest)
        print('✓ %s' % os.path.basename(dest))
        return 0
    dest = os.path.join(out, archive_name(g, version, name))
    if name in DESKTOP:
        files = desktop_files(repo, build_root, g, name)
        check_present('%s %s' % (g['id'], name), files)
        glibc = glibc_of([src for _, src in files]) if name.startswith('linux-') else None
        readme = render(g, version, kind, [name], present_of(g, absent), glibc)
        write_archive(dest, files, readme)
    else:
        e = extra(g, name)
        if 'command' in e:
            with tempfile.TemporaryDirectory() as tmp:
                argv = [a.replace('{out}', tmp).replace('{python}', sys.executable) for a in e['command']]
                subprocess.run(argv, cwd=repo, check=True, stdout=subprocess.DEVNULL)
                made = glob.glob(os.path.join(tmp, e['output']))
                if len(made) != 1:
                    fail('%s: the command made %d files matching %s' % (name, len(made), e['output']))
                shutil.copyfile(made[0], dest + '.part')
                os.replace(dest + '.part', dest)
            print('✓ %s (%s)' % (os.path.basename(dest), os.path.basename(made[0])))
            return 0
        files = extra_files(repo, build_root, e)
        check_present('%s %s' % (g['id'], name), files)
        if e.get('sha256'):
            bad = sum_mismatches(files, read_sums(os.path.join(repo, e['sha256'])))
            if bad:
                fail('%s: the files are not the ones %s lists: %s' % (name, e['sha256'], '; '.join(bad)))
        write_archive(dest, files)
    print('✓ %s: %d file(s) — %s' % (os.path.basename(dest), len(files),
                                     ' '.join(os.path.basename(a) for a, _ in sorted(files))))
    return 0


def cmd_copy_release(repo, g, name, version, out, github_repo):
    e = extra(g, name)
    if not e.get('fromRelease') or not e.get('sha256'):
        fail('%s is not copied from a release (no fromRelease and sha256)' % name)
    sums = read_sums(os.path.join(repo, e['sha256']))
    where = ['--repo', github_repo] if github_repo else []
    r = subprocess.run(['gh', 'release', 'view'] + where + ['--json', 'tagName', '--jq', '.tagName'],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, cwd=repo)
    tag = r.stdout.strip()
    if r.returncode != 0 or not tag:
        print('absent: %s: no versioned release to copy from (%s)' % (name, r.stderr.strip()))
        return EXIT_ABSENT
    with tempfile.TemporaryDirectory() as tmp:
        dl, unpacked = os.path.join(tmp, 'download'), os.path.join(tmp, 'files')
        for pattern in e['fromRelease']:
            subprocess.run(['gh', 'release', 'download', tag] + where + ['--pattern', pattern, '--dir', dl],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=repo)
            zips = sorted(glob.glob(os.path.join(dl, '*.zip')))
            if zips:
                break
        if not zips:
            print('absent: %s: release %s has no %s' % (name, tag, ' or '.join(e['fromRelease'])))
            return EXIT_ABSENT
        files = []
        for z in zips:
            with zipfile.ZipFile(z) as zf:
                for n in zf.namelist():
                    rel = n[len('System/'):] if n.startswith('System/') else None
                    if rel in sums and not any(a == n for a, _ in files):
                        dest = os.path.join(unpacked, *n.split('/'))
                        os.makedirs(os.path.dirname(dest), exist_ok=True)
                        with zf.open(n) as src, open(dest, 'wb') as f:
                            shutil.copyfileobj(src, f)
                        files.append((n, dest))
        bad = sum_mismatches(files, sums)
        if bad:
            print('absent: %s: release %s does not hold the files %s lists: %s' % (name, tag, e['sha256'],
                                                                                  '; '.join(bad)))
            return EXIT_ABSENT
        os.makedirs(out, exist_ok=True)
        dest = os.path.join(out, archive_name(g, version, name))
        write_archive(dest, files)
    print('✓ %s: %d file(s) from release %s, as %s lists them' % (os.path.basename(dest), len(files), tag,
                                                                    e['sha256']))
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('command', choices=('version', 'game', 'archives', 'check', 'list', 'archive', 'notes',
                                        'copy-release'))
    ap.add_argument('archive', nargs='?')
    ap.add_argument('--repo', default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                                    '..', '..')))
    ap.add_argument('--build-root')
    ap.add_argument('--version')
    ap.add_argument('--out')
    ap.add_argument('--kind', choices=('test', 'release'), default='test')
    ap.add_argument('--absent', action='append', default=[])
    ap.add_argument('--glibc-from', action='extend', nargs='+', default=[])
    ap.add_argument('--github-repo')
    a = ap.parse_args(argv)
    if a.archive and a.command not in ('list', 'archive', 'copy-release'):
        fail('%s takes no archive name (got %s)' % (a.command, a.archive))
    repo = os.path.abspath(a.repo)
    build_root = os.path.abspath(a.build_root or repo)
    data = load(repo)
    if a.command == 'check':
        return cmd_check(repo, data)
    if a.command == 'version':
        print(version_of(repo, data))
        return 0
    g = game_of(repo, data)
    if a.command == 'game':
        print(g['id'])
        return 0
    if a.command == 'archives':
        print('\n'.join(list(DESKTOP) + extras_of(g)))
        return 0
    if a.command in ('list', 'archive', 'copy-release') and not a.archive:
        fail('%s needs an archive name' % a.command)
    if a.command == 'list':
        for arc, src in sorted(files_of(repo, build_root, g, a.archive)):
            print('%s  %s' % (arc, src))
        return 0
    if not a.version:
        fail('%s needs --version' % a.command)
    if a.command == 'notes':
        linux = [p for p in DESKTOP if p.startswith('linux-')]
        sources = a.glibc_from or [src for p in linux if os.path.isdir(os.path.join(build_root,
                                                                                    g['platforms'][p]['build']))
                                   for _, src in desktop_files(repo, build_root, g, p)]
        sys.stdout.write(render(g, a.version, a.kind, list(DESKTOP), present_of(g, a.absent), glibc_of(sources)))
        return 0
    if not a.out:
        fail('%s needs --out' % a.command)
    if a.command == 'archive':
        return cmd_archive(repo, build_root, g, a.archive, a.version, a.out, a.kind, a.absent)
    return cmd_copy_release(repo, g, a.archive, a.version, a.out, a.github_repo)


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except Fail as e:
        sys.stderr.write('package.py: %s\n' % e)
        sys.exit(1)
