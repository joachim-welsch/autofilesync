# LoggingDialog

## choices

Index maps directly onto LogLevel (see the static_cast in TransferDataFromWindow), so the order
must stay the one of the enum.

## labelWidth

One label column for the whole page, measured from the longest label: a fixed pixel width clips
as soon as the font or the DPI differs, and a label only as wide as itself starts every field at
a different place.

## onPickBackupProofPath

Offers directories, not files: the setting names the directory the proofs go into, one file per
sync item. A directory that does not exist yet is allowed - writeCopyLog() creates it, and the
dialog can create it here as well.

## updateNotifyEnabling

The address stays readable while switched off - greying it out says "not in use" without hiding
what was configured.

## proof-path-not-in-logging

Not part of Logging: the proofs are the application's own artefact, and Logging is the shared
library the daemon logs through as well.

## smtp-compared-as-whole

Compared as a whole: the account is stored as one value, so a per-field comparison would only
repeat what operator!= already answers.

## Validate

"Aktiv" without a recipient would look like a working notification and silently deliver nothing
- exactly what one does not want to find out about after the error it was meant to report.

## server-required

The notification goes out over this server; without it the switch above would again promise
something that never arrives.
