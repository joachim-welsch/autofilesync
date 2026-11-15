# Hex

Module comment for Hex.h / Hex.cpp.

## encoded

Two lowercase digits per byte, nothing in between. Lowercase is part of the contract: the AWS
signature (S3.md) is compared byte by byte by the server, and device ids and certificate
fingerprints are compared as text with what the phone sends.
