# vim: set ft=python

Import('env prefix')

PIXMAN_VERSION_MAJOR=0
PIXMAN_VERSION_MINOR=15
PIXMAN_VERSION_MICRO=21
PIXMAN_VERSION_STRING="%d.%d.%d" % (PIXMAN_VERSION_MAJOR, PIXMAN_VERSION_MINOR, PIXMAN_VERSION_MICRO)

env['DOT_IN_SUBS'] = {'@PACKAGE_VERSION@': PIXMAN_VERSION_STRING,
					  '@VERSION@': PIXMAN_VERSION_STRING,
					  '@PIXMAN_VERSION_MAJOR@': str(PIXMAN_VERSION_MAJOR),
					  '@PIXMAN_VERSION_MINOR@': str(PIXMAN_VERSION_MINOR),
					  '@PIXMAN_VERSION_MICRO@': str(PIXMAN_VERSION_MICRO),
					  '@prefix@': prefix,
 					  '@exec_prefix@': '${prefix}/bin',
					  '@libdir@': '${prefix}/lib',
					  '@includedir@': '${prefix}/include',
					  '@DEP_CFLAGS@': '',
					  '@DEP_LIBS@': ''}

env.DotIn('pixman-1.pc', 'pixman-1.pc.in')
env.DotIn('config.h', 'win32/config.h.in')

env.Append(CPPDEFINES='HAVE_CONFIG_H')
env.Append(CPPPATH=['..', '.'])

ip = env.Install(prefix + '/lib/pkgconfig', 'pixman-1.pc')

SConscript('pixman/SConscript', exports=['env', 'prefix'])

env.Depends('pixman/SConscript', 'config.h')

Alias('install', [ip])

env.Depends('install', ['pixman-1.pc'])
