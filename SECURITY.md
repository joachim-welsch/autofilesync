# Security Policy

AutoFileSync copies, overwrites and deletes files and connects to remote servers over SSH/SFTP. Security reports are taken seriously, and so are bugs that can silently destroy data.

## Supported versions

Security fixes are provided for the latest release and for the current `main` branch. Older releases are not patched; please update to the latest version.

## Reporting a vulnerability

**Please do not open a public issue for security problems.**

Use GitHub's private vulnerability reporting instead:

1. Go to the [Security tab](https://github.com/joachim-welsch/autofilesync/security) of the repository.
2. Click **Report a vulnerability**.
3. Fill in the form.

Please include:

- AutoFileSync version or commit hash
- Operating system and version
- A description of the problem and its impact
- Steps to reproduce, ideally with a minimal example
- Whether the issue is already known to others

Remove real host names, user names, keys and passwords from logs and examples.

## What to expect

AutoFileSync is maintained by one person in spare time. You can expect:

- an acknowledgement within 7 days,
- an assessment and a rough plan within 30 days,
- a fix as soon as reasonably possible, depending on severity and complexity.

Please allow up to 90 days before public disclosure, or coordinate a date in the report. You will be credited in the release notes unless you prefer to stay anonymous.

There is no bug bounty programme.

## Scope

In scope are issues in AutoFileSync itself, for example:

- exposure of passwords, passphrases or private keys (in memory dumps, logs, configuration files or temporary files)
- weaknesses in SSH host key verification or authentication handling
- path traversal, symlink or file name handling that lets a remote server or a crafted directory tree write, read or delete files outside the configured sync folders
- memory safety errors (buffer overflows, use-after-free) reachable through file names, file contents, configuration files or server responses
- privilege escalation or unsafe file permissions on created files

Out of scope:

- vulnerabilities in third-party libraries (wxWidgets, libssh2, OpenSSL, Boost) without an AutoFileSync-specific impact – please report those upstream; if AutoFileSync ships or requires a vulnerable version, that *is* worth reporting here
- attacks that require an already compromised local user account or administrator access
- problems caused by a deliberately insecure configuration chosen by the user
- social engineering and physical access

## Data-loss bugs

A bug that causes wrong files to be overwritten or deleted without any attacker involved is not a vulnerability in the strict sense, but it is treated with the same priority. Report it as a normal issue and put **data loss** in the title. If you are unsure whether it is exploitable by a third party, use the private channel.

## Security updates

Fixes are announced in the release notes on GitHub. Published advisories appear under the repository's Security tab.
