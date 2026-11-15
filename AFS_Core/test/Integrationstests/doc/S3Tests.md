# S3Tests

Module comment for S3Tests.cpp.

## S3Test

Each test starts a stand-in for an S3 service (WebDavTestServer.md#S3TestServer) with two
empty buckets and adds an account for it to the configuration. Objects are prepared and
inspected through the server's test addresses, which need no signature. Without python3 the
tests are skipped.

## on_server

Every object in every bucket, as "bucket/key".

## PagedS3Test

A server that returns two entries per page.

## signature

Every request of every test here is only answered when its signature is right, so all of them
test the signing. This test says how strong that is: when botocore is installed, the server
also computes each signature with botocore, which shares no code with the program or with the
server, and the test passes; without botocore it is reported as skipped.

## refused

A wrong secret key, a wrong region and an unknown access key each have to be an error, also
for the question whether a file exists.

## folders

A folder exists through the keys below it or through a marker of its own. A name that is only
the beginning of a folder's name is not that folder.

## difficult-names

A blank, an apostrophe, umlauts, an ampersand, a percent sign, brackets, a plus and an equals
sign: each is encoded in the address, and the encoding is part of what is signed, so a wrong
encoding shows as a refused signature.

## time

The modification time travels as metadata (S3.md#modification-time); the server reports an
upload time far in the future, so a test that read the wrong time would fail.

## LargeFileIsUploadedInParts

Thirty-three mebibytes: three parts. The server refuses to complete an upload whose list of
parts does not match what it received.

## move

The moved file must arrive with its content and its modification time, the old key must be
gone, and a taken name must stay untouched.
