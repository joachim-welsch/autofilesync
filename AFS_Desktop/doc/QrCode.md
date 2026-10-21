# QrCode

Module comment for QrCode.h / QrCode.cpp.

## qr_bitmap

Renders the text with Nayuki's qrcodegen (third-party/qrcodegen, MIT) into a square bitmap:
black modules on white, with the four-module quiet zone the standard requires and an integer
scale so that every module is crisp - a scaled module boundary between two pixels is what
makes a camera misread a code. Error correction level M: the pairing URL is about 200
characters, and M keeps the code readable from a screen at arm's length while staying small
enough for a dialog.
