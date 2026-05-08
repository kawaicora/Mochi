for /f "usebackq delims=" %%i in (`
    "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" ^
    -latest ^
    -requires Microsoft.Component.MSBuild ^
    -find MSBuild\**\Bin\MSBuild.exe
`) do (
    set "MSBUILD=%%i"
)

"%MSBUILD%" ..\Mochi.sln ^
 /m ^
 /p:Configuration=Debug ^
 /p:Platform=x86