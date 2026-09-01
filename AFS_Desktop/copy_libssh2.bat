@echo off
if "%1"=="Debug" (
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\libssh2.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\zlibd1.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\llama.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\ggml.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\ggml-base.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\ggml-cpu.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\libssl-3-x64.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\debug\bin\libcrypto-3-x64.dll" "%2" >nul 2>&1
) else (
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\libssh2.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\zlib1.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\llama.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\ggml.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\ggml-base.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\ggml-cpu.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\libssl-3-x64.dll" "%2" >nul 2>&1
    copy /Y "C:\Entwicklung\extlib\vcpkg\installed\x64-windows\bin\libcrypto-3-x64.dll" "%2" >nul 2>&1
)
