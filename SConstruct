# vim: ft=python expandtab

import os
from site_init import GBuilder, Initialize

opts = Variables()
opts.Add(PathVariable('PREFIX', 'Installation prefix', os.path.expanduser('~/FOSS'), PathVariable.PathIsDirCreate))
opts.Add(BoolVariable('DEBUG', 'Build with Debugging information', 0))
opts.Add(PathVariable('PERL', 'Path to the executable perl', r'C:\Perl\bin\perl.exe', PathVariable.PathIsFile))

env = Environment(variables = opts,
                  ENV=os.environ, tools = ['default', GBuilder])

Initialize(env)

PIXMAN_VERSION_MAJOR=0
PIXMAN_VERSION_MINOR=15
PIXMAN_VERSION_MICRO=21
PIXMAN_VERSION_STRING="%d.%d.%d" % (PIXMAN_VERSION_MAJOR, PIXMAN_VERSION_MINOR, PIXMAN_VERSION_MICRO)

env['DOT_IN_SUBS'] = {'@PACKAGE_VERSION@': PIXMAN_VERSION_STRING,
					  '@VERSION@': PIXMAN_VERSION_STRING,
					  '@PIXMAN_VERSION_MAJOR@': str(PIXMAN_VERSION_MAJOR),
					  '@PIXMAN_VERSION_MINOR@': str(PIXMAN_VERSION_MINOR),
					  '@PIXMAN_VERSION_MICRO@': str(PIXMAN_VERSION_MICRO),
					  '@prefix@': env['PREFIX'],
 					  '@exec_prefix@': '${prefix}/bin',
					  '@libdir@': '${prefix}/lib',
					  '@includedir@': '${prefix}/include',
					  '@DEP_CFLAGS@': '',
					  '@DEP_LIBS@': ''}

env.DotIn('pixman-1.pc', 'pixman-1.pc.in')
env.DotIn('config.h', 'win32/config.h.in')

env.Append(CPPDEFINES='HAVE_CONFIG_H')
env.Append(CPPPATH=['#pixman', '#'])
env.Append(CFLAGS=env['DEBUG_CFLAGS'])
env.Append(CPPDEFINES=env['DEBUG_CPPDEFINES'])

env.Alias('install', env.Install('$PREFIX/lib/pkgconfig', 'pixman-1.pc'))

SConscript(['pixman/SConscript', 'test/SConscript'], exports=['env'])
