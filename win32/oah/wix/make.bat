@echo off
set RELEASE_PATH=%~dp0\..\Win32\Release
%OAH_INSTALLED_PATH%bin\pkg-config --modversion %RELEASE_PATH%\lib\pkgconfig\pixman-1.pc > libver.tmp || goto error
set /P LIBVER= < libver.tmp
del libver.tmp

nmake /nologo version=%LIBVER% release_path=%RELEASE_PATH% %*

goto:eof
:error
echo Couldn't start build process... have you compiled pixman.sln with OAH_BUILD_OUTPUT cleared!??