#!/usr/bin/env python
# encoding: utf-8
# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4

from waflibs import *
from waflib.Configure import conf, ConfigurationError
from waflibs.github.autoconf.defaults import INCLUDES_DEFAULT
from waflib import Context
from waflib import Utils
import re
import os

out = 'debug'
top = '.'

pixman_major = 0
pixman_minor = 19
pixman_micro = 3

VERSION= "%d.%d.%d" % (pixman_major, pixman_minor, pixman_micro)
APPNAME = 'pixman'

DECL_CODE = '''
int
main ()
{
#ifndef %s
#error Not declared
#endif

  return 0;
}
'''
OPENMP_CODE = r'''
#include <stdio.h>

extern unsigned int lcg_seed;
#pragma omp threadprivate(lcg_seed)
unsigned int lcg_seed;

unsigned function(unsigned a, unsigned b)
{
    lcg_seed ^= b;
    return ((a + b) ^ a ) + lcg_seed;
}

int main(int argc, char **argv)
{
    int i;
    int n1 = 0, n2 = argc;
    unsigned checksum = 0;
    int verbose = argv != NULL;
    unsigned (*test_function)(unsigned, unsigned);
    test_function = function;
#pragma omp parallel for reduction(+:checksum) default(none) \
    shared(n1, n2, test_function, verbose)
    for (i = n1; i < n2; i++)
        {
        unsigned crc = test_function (i, 0);
        if (verbose)
            printf ("%d: %08X\n", i, crc);
        checksum += crc;
    }
    printf("%u\n", checksum);
    return 0;
}
'''

MMX_CODE='''
#if defined(__GNUC__) && (__GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 4))
error "Need GCC >= 3.4 for MMX intrinsics"
#endif
#include <mmintrin.h>
int main () {
    __m64 v = _mm_cvtsi32_si64 (1);
    return _mm_cvtsi64_si32 (v);
}
'''

SSE2_CODE='''
#if defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 2))
#   if !defined(__amd64__) && !defined(__x86_64__)
#      error "Need GCC >= 4.2 for SSE2 intrinsics on x86"
#   endif
#endif
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>
int main () {
    __m128i a = _mm_set1_epi32 (0), b = _mm_set1_epi32 (0), c;
    c = _mm_xor_si128 (a, b);
    return 0;
}
'''

VMX_CODE = r'''
#if defined(__GNUC__) && (__GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 4))
error "Need GCC >= 3.4 for sane altivec support"
#endif
#include <altivec.h>
int main () {
    vector unsigned int v = vec_splat_u32 (1);
    v = vec_sub (v, v);
    return 0;
}
'''

ARM_SIMD_CODE = r'''
.text
.arch armv6
.object_arch armv4
.arm
.altmacro
#ifndef __ARM_EABI__
#error EABI is required (to be sure that calling conventions are compatible)
#endif
pld [r0]
uqadd8 r0, r0, r0
'''

ARM_NEON_CODE = r'''
.text
.fpu neon
.arch armv7a
.object_arch armv4
.eabi_attribute 10, 0
.arm
.altmacro
#ifndef __ARM_EABI__
#error EABI is required (to be sure that calling conventions are compatible)
#endif
pld [r0]
vmovn.u16 d0, q0
'''

GNUC_INLINE_ASM_CODE = r'''
int main () {
    /* Most modern architectures have a NOP instruction, so this is a fairly generic test. */
	asm volatile ( "\tnop\n" : : : "cc", "memory" );
    return 0;
}
'''
THREAD_LOCAL_CODE = r'''
#ifdef __MINGW32__
#error MinGW has broken __thread support
#endif
#ifdef __OpenBSD__
#error OpenBSD has broken __thread support
#endif
static __thread int x ;
int main () { x = 123; return x; }
'''

TLS_CODE = r'''
#include <stdlib.h>
#include <pthread.h>

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_key_t key;

static void
make_key (void)
{
    pthread_key_create (&key, NULL);
}

int
main ()
{
    void *value = NULL;

    if (pthread_once (&once_control, make_key) != 0)
    {
	value = NULL;
    }
    else
    {
	value = pthread_getspecific (key);
	if (!value)
	{
	    value = malloc (100);
	    pthread_setspecific (key, value);
	}
    }
    return 0;
}
'''

unsupported_options = re.compile(r'Command line warning \w* : ignoring unknown option')

@conf
def check_cflags(self, flag, code='', **kw):
    kw.update({'fragment': code + '\nint main(int c, char **v) { (void)c; (void)v; return 0; }',
              'msg': 'Checking whether compiler supports ' + flag,
              'compiler': 'c',
              'msvc_warn': unsupported_options,
              'cflags': flag,
              'ccflags': flag})
    self.check_compile_warn(**kw)

def options(opt):
    cfg = opt.parser.get_option_group('--prefix')
    bld = opt.parser.get_option_group('-p')

    cfg.add_option('--host', action='store', default=None, dest='host', help='cross-compile to build programs to run on HOST')
    cfg.add_option('--disable-mmx', action='store_false', dest='mmx', default=None, help='disable MMX fast paths (default:auto)')
    cfg.add_option('--disable-sse2', action='store_false', dest='sse2', default=None, help='disable SSE2 fast paths (default:auto)')
    cfg.add_option('--disable-vmx', action='store_false', dest='vmx', default=None, help='disable VMX fast paths (default:auto)')
    cfg.add_option('--disable-arm-simd', action='store_false', dest='arm_simd', default=None, help='disable ARM SIMD fast paths (default:auto)')
    cfg.add_option('--disable-arm-neon', action='store_false', dest='arm_neon', default=None, help='disable ARM NEON fast paths (default:auto)')
    cfg.add_option('--enable-timers', action='store_true', dest='timers', default=False, help='enable TIMER_BEGIN and TIMER_END macros (default:no)')
    cfg.add_option('--enable-gtk', action='store_true', dest='gtk', default=None, help='enable tests using GTK+ (default:auto)')
    opt.tool_options("compiler_c")
    opt.tool_options("perl")

def configure(cfg):
    platinfo = PlatInfo()
    cfg.start_msg('Checking build system type:')
    cfg.end_msg(platinfo.fullname())

    cfg.start_msg('Checking host system type:')
    cfg.env.host = cfg.options.host
    if not cfg.env.host:
        cfg.env.host = platinfo
    else:
        cfg.env.host = PlatInfo.from_name(cfg.options.host)
    cfg.end_msg(cfg.env.host.fullname())

    cfg.check_tool('compiler_c')
    try:
        cfg.check_cc(fragment='int main(){return 0;}', execute=True, msg='Checking whether cross-compiling', okmsg='no', errmsg='yes')
        cfg.env.cross_compile=False
    except:
        cfg.env.cross_compile=True
    cfg.check_tool('compiler_c')
    cfg.check_cc(function_name='getisax', mandatory=False)
    cfg.check_endian()
    cfg.check_inline()
    env = cfg.env.derive()
    pixman_werror = None
    for flags in ('-Werror', '-errwarn'):
        try:
            cfg.check_cflags(flags)
            pixman_werror = flags
            break
        except:
            continue
    try:
        cfg.check_cc(fragment=INCLUDES_DEFAULT + DECL_CODE % '__amd64', msg='Checking whether __amd64 is defined') 
        AMD64_ABI = True
    except:
        try:
            cfg.check_cc(fragment=INCLUDES_DEFAULT + DECL_CODE % '_WIN64', msg='Checking whether _WIN64 is defined') 
            AMD64_ABI = True
        except:
            AMD64_ABI = False

    if cfg.env.CC_NAME == 'sun':
        # Default CFLAGS to -O -g rather than just the -g from AC_PROG_CC
        # if we're using Sun Studio and neither the user nor a config.site
        # has set CFLAGS.
        if cfg.env.CFLAGS == ['-g']:
            cfg.env.CFLAGS = ['-O', '-g']

        cfg.check_sizeof('long')

    cfg.check_cflags('-Wall', mandatory=False)
    cfg.check_cflags('-fno-strict-aliasing', mandatory=False)

    cfg.check_tool('perl')
    if not cfg.check_perl_version():
        self.fatal('Perl is required to build ' + APPNAME)
    try:
        cfg.check_openmp_cflags(uselib_store='OPENMP')
        env = cfg.env.derive()
        env.append_value('CCFLAGS', getattr(cfg.env, 'OPENMP_CFLAGS', []))
        env.append_value('LINKFLAGS', getattr(cfg.env, 'OPENMP_LINKFLAGS', []))
        cfg.check_cc(fragment=OPENMP_CODE, msg='Checking OpenMP support', define_name='USE_OPENMP', env=env)
    except ConfigurationError:
        pass
    
    try:
        cfg.check_cflags('-fvisibility=hidden', 
'''#if defined(__GNUC__) && (__GNUC__ >= 4)
#else\n error Need GCC 4.0 for visibility\n
#endif\n''')
    except ConfigurationError:
        cfg.check_cflags('-xldscope=hidden', '''#if defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#else
error Need Sun Studio 8 for visibility
#endif''', mandatory=False)


    #==========================================
    #Check for MMX
    if not getattr(cfg.env, 'MMX_CFLAGS', None):
        if 'sun' in cfg.env.CC_NAME:
            # Sun Studio doesn't have an -xarch=mmx flag, so we have to use sse
            # but if we're building 64-bit, mmx & sse support is on by default and
            # -xarch=sse throws an error instead
            if not AMD64_ABI:
                self.env.MMX_CFLAGS = ['-xarch=see']
    else:
        cfg.env.MMX_CFLAGS = ['-mmmx', '-Winline']
    
    if cfg.options.mmx != False:
        try:
            cfg.check_cc(fragment=MMX_CODE, uselib_store='MMX', msg='Checking for mmx support', ccflags=cfg.env.MMX_CFLAGS, define_name='USE_MMX')
        except ConfigurationError:
            cfg.env.MMX_CFLAGS = []
            if cfg.options.mmx:
                cfg.fatal('MMX intrinsics not detected')
    #==========================================
    #Check for SSE2
    if not getattr(cfg.env, 'SSE2_CFLAGS', None):
        if 'sun' in cfg.env.CC_NAME:
            # Sun Studio doesn't have an -xarch=mmx flag, so we have to use sse
            # but if we're building 64-bit, mmx & sse support is on by default and
            # -xarch=sse throws an error instead
            if not AMD64_ABI:
                self.env.SSE2_CFLAGS = ['-xarch=see']
    else:
        cfg.env.SSE2_CFLAGS = ['-mmmx', '-msse2', '-Winline']
    if cfg.options.sse2 != False:
        try:
            cfg.check_cc(fragment=SSE2_CODE, uselib_store='SSE2', msg='Checking for sse2 support', ccflags=cfg.env.SSE2_CFLAGS, define_name='USE_SSE2')
        except ConfigurationError:
            cfg.env.SSE2_CFLAGS = []
            if cfg.options.sse2:
                cfg.fatal('SSE2 intrinsics not detected')

    #============================================================
    # Other special flags needed when building code using MMX or SSE instructions
    if cfg.env.host.os == 'solaris':
          # When building 32-bit binaries, apply a mapfile to ensure that the
          # binaries aren't flagged as only able to run on MMX+SSE capable CPUs
          # since they check at runtime before using those instructions.
          # Not all linkers grok the mapfile format so we check for that first.
        if not AMD64_ABI:
            HWCAP_LINKFLAGS='-Wl,-M,'+ os.path.join(os.path.dirname(Context.g_module.root_path), 'pixman', 'solaris-hwcap.mapfile')
            try:
                cfg.check_cc(linkflags=HWCAP_LINKFLAGS, msg='Checking whether to use a hardware capability map file', errmsg='no')
                if getattr(cfg.env, 'MMX_LINKFLAGS', None):
                    self.env.MMX_LINKFLAGS = HWCAP_LINKFLAGS
                if getattr(cfg.env, 'SSE2_LINKFLAGS', None):
                    cfg.env.SSE2_LINKFLAGS = HWCAP_LINKFLAGS
            except ConfigurationError:
                pass
    #================================================================
    #Check for VMX/Altivec
    if cfg.env.CC_NAME != 'msvc': 
        ver = cfg.cmd_and_log(Utils.subst_vars('"${CC}" -v 2>&1', cfg.env))
        for x in ver.split('\n'):
            if x.find('version') > 0 and x.find('Apple') > 0:
                cfg.env.VMX_CFLAGS = '-faltivec'
                break
    if getattr(cfg.env, 'VMX_CFLAGS', []):
        cfg.env.VMX_CFLAGS = '-maltivec -mabi=altivec'
        
    if cfg.options.vmx != False:
        try:
            cfg.check_cc(fragment=VMX_CODE, uselib_store='VMX', msg='Checking for vmx support', ccflags=cfg.env.VMX_CFLAGS, define_name='USE_VMX')
        except ConfigurationError:
            cfg.env.VMX_CFLAGS = []
            if cfg.options.vmx:
                cfg.fatal('VMX intrinsics not detected')
    # ==========================================================================
    # Check if assembler is gas compatible and supports ARM SIMD instructions
    if cfg.options.arm_simd != False:
        cfg.check_cc(fragment=ARM_SIMD_CODE, ccflags='-x assembler-with-cpp $CFLAGS', msg='Checking whether to use ARM SIMD assembler', define_name='USE_ARM_SIMD', mandatory=False)

    # ==========================================================================
    # Check if assembler is gas compatible and supports NEON instructions
    if cfg.options.arm_neon != False:
        cfg.check_cc(fragment=ARM_NEON_CODE, ccflags='-x assembler-with-cpp $CFLAGS', msg='Checking whether to use ARM NEON assembler', define_name='USE_ARM_NEON', mandatory=False)

    # =========================================================================================
    # Check for GNU-style inline assembly support
    cfg.check_cc(fragment=GNUC_INLINE_ASM_CODE, msg='Checking whether to use GNU-style inline assembler', define_name='USE_GCC_INLINE_ASM', mandatory=False)

    cfg.define_cond('PIXMAN_TIMERS', cfg.options.timers)

    # ===================================
    # GTK+
    if cfg.options.gtk in (True, None):
        try:
            cfg.check_cc(lib='pixman-1', function_name='pixman_version_string')
            cfg.check_cfg(['gtk+-2.0'])
            cfg.check_cfg(['pixman-1'])
            cfg.define('HAVE_GTK', 1)
        except ConfigurationError:
            pass
    # =====================================
    # posix_memalign, sigaction, alarm
    cfg.check_cc(function_name='posix_memalign', mandatory=False)
    cfg.check_cc(function_name='sigaction', mandatory=False)
    cfg.check_cc(function_name='alarm', mandatory=False)

    # =====================================
    # Thread local storage
    try:
        cfg.check_cc(fragment=THREAD_LOCAL_CODE, define_name='TOOLCHAIN_SUPPORTS__THREAD', msg='Checking for __thread')

        # posix tls
        cfg.start_msg('Checking for pthread_setspecific')
        kw = {'fragment': TLS_CODE, 'ccflags': '-D_REENTRANT', 'libs': '-lpthread', 'compiler': 'c', 'define_name': 'HAVE_PTHREAD_SETSPECIFIC', 'uselib_store': 'PTHREAD'}
        try:
            cfg.validate_c(kw)
            cfg.run_c_code(**kw)
            cfg.post_check(**kw)
            cfg.end_msg('yes')
        except ConfigurationError:
            kw.update({'ccflags': '-pthread', 'linkflags': '-pthread'})
            del kw['libs']
            cfg.validate_c(kw)
            cfg.run_c_code(**kw)
            cfg.post_check(**kw)
            cfg.end_msg('yes')
    except ConfigurationError:
        cfg.end_msg('no', 'YELLOW')

    cfg.write_config_header('config.h')
    #print ("env = %s" % cfg.env)
    #print ("options = ", cfg.options)


def build(bld):
    bld(source=['pixman-1.pc.in', 'pixman-1-uninstalled.pc.in'])
