@echo off
echo Building MPI IQ Solver...

REM Check if cl is available
cl >nul 2>&1
if %ERRORLEVEL% EQU 0 goto :build_with_cl

REM Setup Visual Studio environment
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto :build_with_cl
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    goto :build_with_cl
)

echo ERROR: Visual Studio not found!
echo Install Visual Studio Community or run from Developer Command Prompt.
pause
exit /b 1

:build_with_cl
REM Maximum optimization equivalent to: gcc -O3 -march=native -flto -pipe -std=c11
cl /O2 /Ox /Oi /Ot /Oy /GL /GS- /DNDEBUG /std:c11 /favor:INTEL64 ^
   /I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" ^
   iq_mpi.c init.c ^
   /link /LTCG /OPT:REF /OPT:ICF ^
   /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" ^
   msmpi.lib /OUT:iq_mpi.exe

if %ERRORLEVEL% EQU 0 (
    echo SUCCESS! Created: iq_mpi.exe
    echo To run: mpiexec -n 4 iq_mpi.exe
) else (
    echo BUILD FAILED!
    echo Ensure MS-MPI SDK is installed.
)
pause