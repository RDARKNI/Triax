# Security Policy

## Supported versions

Security fixes are applied to the latest release and the current `main` branch. Older releases may not receive backported fixes.

## Reporting a vulnerability

Please do not report suspected security vulnerabilities through a public issue, discussion, or pull request.

Instead, use GitHub's private vulnerability-reporting feature:

1. Open the repository's **Security** page.
2. Select **Report a vulnerability**.
3. Submit the private report.

Please include the affected Triax version, operating system and compiler, a description of the potential impact, and instructions or code that reproduce the issue.

## Scope

Triax executes user-provided native code. Its process-isolation feature is intended to contain accidental crashes, exits and hangs; it is **not a security sandbox** and must not be used to execute untrusted code.

Potential security issues include unintended behavior in the parent runner, unsafe handling of paths or report data, and failures that cross the intended process-isolation boundary.

Ordinary assertion failures, expected test-process crashes, portability problems and other correctness bugs should be reported through the public issue tracker.
