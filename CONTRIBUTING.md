# Contributing to AutoFileSync

Thank you for your interest in AutoFileSync. Bug reports, fixes, tests, documentation and translations are all welcome.

AutoFileSync is a cross-platform desktop file synchronisation tool written in C++. A sync tool handles other people's data, so correctness and data safety come before features and before speed. Please keep that in mind when proposing changes.

## Ways to contribute

- **Report a bug** – open an issue (see [Reporting bugs](#reporting-bugs)).
- **Suggest a feature** – open an issue describing the use case before writing code.
- **Fix something** – small fixes can go straight to a pull request.
- **Improve tests** – especially for conflict handling, interrupted transfers and platform differences.
- **Improve documentation** – README, build instructions, user-facing texts.

For anything larger than a small fix, please open an issue first so we can agree on the approach before you invest time.

## Reporting bugs

Please include:

- AutoFileSync version or commit hash
- Operating system and version (Linux distribution, macOS, Windows)
- Local or remote (SFTP) sync, and the server type if remote
- What you did, what you expected, what happened
- Relevant log output

**Remove private data first**: host names, user names, paths, and anything that looks like a key or password.

If the bug caused or could cause data loss, say so in the title.

### Security issues

Do not report security vulnerabilities in public issues. Use GitHub's private vulnerability reporting ("Report a vulnerability" under the repository's *Security* tab) instead.

## Development setup

### Dependencies

- A C++20 compiler (GCC, Clang, or MSVC from Visual Studio 2022)
- CMake
- wxWidgets
- libssh2
- OpenSSL
- Boost
- Google Test (for the tests)

### Linux / macOS

Install the dependencies with your package manager, then:

```sh
git clone https://github.com/joachim-welsch/autofilesync.git
cd autofilesync
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Windows

Use Visual Studio 2022 with [vcpkg](https://vcpkg.io/) for the dependencies:

```powershell
git clone https://github.com/joachim-welsch/autofilesync.git
cd autofilesync
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Debug
```

### Running the tests

```sh
ctest --test-dir build --output-on-failure
```

On Windows add `-C Debug`.

All tests must pass before you open a pull request. If a test is flaky on your platform, report it as a bug rather than disabling it.

## Making changes

1. Fork the repository and create a branch from `main`:
   `git checkout -b fix/short-description`
2. Make your change in small, focused commits.
3. Add or update tests for the behaviour you changed.
4. Build and run the tests on at least one platform.
5. Open a pull request.

### Code guidelines

- Follow the style of the surrounding code; do not reformat unrelated lines.
- Target C++20. Prefer the standard library, then Boost, before adding a new dependency. New dependencies need prior discussion.
- Keep the code portable. Avoid raw POSIX or Win32 calls outside the existing platform abstraction; if you need one, provide the counterpart for the other platforms or say clearly in the pull request that it is missing.
- Keep the sync core free of GUI code. wxWidgets belongs in the frontend only.
- Use RAII for resources; no owning raw pointers.
- Do not swallow errors. A failed file operation must be reported, never silently skipped.
- Tests must not depend on wall-clock timing finer than the platform guarantees (timer granularity differs on Windows).

### Changes to sync behaviour

Changes that affect what gets copied, overwritten or deleted need extra care:

- Describe the behaviour before and after in the pull request.
- Add tests for the edge cases: conflicts on both sides, interrupted runs, case-insensitive file systems, long paths, unusual file names.
- Never make a destructive operation the default.

### Commit messages

- Short summary line in the imperative ("Fix timestamp comparison on FAT volumes"), at most about 70 characters
- Blank line, then an explanation of *why* if it is not obvious
- Reference issues where relevant (`Fixes #123`)

## Pull requests

- One topic per pull request.
- Explain what the change does and how you tested it, including the platforms.
- Mention anything you could not test.
- Be prepared for review comments; this is a spare-time project, so a reply may take a few days.

## Licence

AutoFileSync is licensed under **GPL-3.0-or-later**. By submitting a contribution you agree that it is licensed under the same terms, and you confirm that you have the right to submit it (it is your own work, or compatible with GPL-3.0-or-later and clearly marked).

New source files should carry the SPDX identifier:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
```

## Conduct

Be respectful and factual. Criticise code, not people.

## Questions

Open an issue, or see <https://welsch-software.com> for contact details.
