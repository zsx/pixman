# vim: set ft=python

Import('env prefix')

env_pixman = env.Clone()

PIXMAN_VERSION_MAJOR=0
PIXMAN_VERSION_MINOR=15
PIXMAN_VERSION_MICRO=21
PIXMAN_VERSION_STRING="%d.%d.%d" % (PIXMAN_VERSION_MAJOR, PIXMAN_VERSION_MINOR, PIXMAN_VERSION_MICRO)

env_pixman['DOT_IN_SUBS'] = {'@PACKAGE_VERSION@': PIXMAN_VERSION_STRING,
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

env_pixman.DotIn('pixman-1.pc', 'pixman-1.pc.in')
env_pixman.DotIn('config.h', 'win32/config.h.in')

env_pixman.Append(CPPDEFINES='HAVE_CONFIG_H')
env_pixman.Append(CPPPATH=['#pixman/pixman', '#pixman'])

ip = env_pixman.Install(prefix + '/lib/pkgconfig', 'pixman-1.pc')

SConscript(['pixman/SConscript', 'test/SConscript'], exports=['env_pixman', 'prefix'])

env_pixman.Depends('pixman/SConscript', 'config.h')

Alias('install', [ip])

env_pixman.Depends('install', ['pixman-1.pc'])
