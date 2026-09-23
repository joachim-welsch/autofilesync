# Version

Module comment for include/Version.h.

## macros

The version is kept as macros although the project prefers constants: the resource script
autofilesync.rc reads the same header for the VERSIONINFO block, and the resource compiler
knows no constexpr. RC_INVOKED hides the C++ part from it. The text is what the installers
read (makeWindowsInstaller.ps1, makeDebianPackage.sh, macos_bundle.sh) and what the About
box shows; the three numbers must agree with it - "0.01.0001" is 0, 1, 1 - because Windows
compares versions numerically and drops the leading zero anyway.
