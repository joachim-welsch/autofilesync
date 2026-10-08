# Version

Module comment for include/Version.h.

## macros

The version is kept as macros although the project prefers constants: the resource script
autofilesync.rc reads the same header for the VERSIONINFO block, and the resource compiler
knows no constexpr. RC_INVOKED hides the C++ part from it. The text is what the installers
read (makeWindowsInstaller.ps1, makeDebianPackage.sh, macos_bundle.sh) and what the About
box shows; the three numbers must agree with it - "0.1.0" is 0, 1, 0 - because Windows and
Debian compare the fields numerically. The scheme is Semantic Versioning: major.minor.patch.
