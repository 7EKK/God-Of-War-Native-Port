@set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
@call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cl /nologo /O2 /W3 switch_throttle.c winmm.lib && del switch_throttle.obj
