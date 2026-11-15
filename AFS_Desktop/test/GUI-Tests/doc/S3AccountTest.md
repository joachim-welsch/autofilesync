# S3AccountTest

Module comment for S3AccountTest.cpp.

## S3AccountTest

Drives "Datei → Neue S3-Verbindung…" against the S3 stand-in of the integration tests
(WebDavTestServer.md#S3TestServer). The fixture's TearDown reads the configuration back from
the file, which removes a connection a test saved.

## buckets

After the checked connection is saved, the directory dialog is opened on it to see that the
root shows the buckets.
