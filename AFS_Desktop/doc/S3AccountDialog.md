# S3AccountDialog

Module comment for S3AccountDialog.h / S3AccountDialog.cpp.

## S3AccountDialog

Creates or edits a connection to an S3 service: a name, the endpoint, the region and the two
keys. There is no sign-in in a browser; the keys are made in the provider's console. The bucket
is not asked for, because it is chosen afterwards like a folder (S3.md#paths).

## address-hint

Endpoint and region are what a newcomer does not know, so the line under the field shows what
they look like at two common providers and says where the keys come from. The text is wrapped
to a fixed width because a wxStaticText does not wrap by itself.

## connection-check

OK asks the service for the list of buckets before anything is saved, which tests the endpoint,
the region and the keys together and shows the service's own message when one of them is wrong.
A key that is restricted to one bucket is not allowed to list buckets although it is valid; the
question whether to save anyway says so.
