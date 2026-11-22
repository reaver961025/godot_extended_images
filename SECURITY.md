# Security Policy

## Supported versions

The latest released version of `godot_extended_images` is the only one that
receives security updates. The `main` branch may contain unreleased fixes.

## Reporting a vulnerability

Please report security issues **privately** rather than as a public GitHub
issue. Open a draft [GitHub Security Advisory](../../security/advisories/new)
on this repository, or email the maintainer directly.

Include:

- A description of the issue and its impact.
- The affected component (extension code in `src/`, or one of the bundled
  third-party libraries under `thirdparty/`).
- A minimal reproducer (a sample image that triggers the bug is ideal).
- The version you tested against.

You should receive an initial response within 7 days.

## Vulnerabilities in bundled third-party libraries

This extension statically links several image-decoding libraries
(libavif, dav1d, libjxl, libheif, libde265, brotli, highway, skcms, qoi).
Vulnerabilities discovered upstream in these libraries are tracked here:

- Track the [GitHub Dependabot alerts](../../security/dependabot) for this
  repository (Dependabot is enabled).
- The versions currently bundled are listed in `LICENSE` (Third-party
  licenses table) and in the project's `CHANGELOG.md`.
- When a CVE is fixed upstream, the affected library is bumped and a new
  release is cut with a note in `CHANGELOG.md`.

## Hardening recommendations for untrusted input

If you load image files that come from untrusted sources (user-uploaded
content, mods, network), be aware that:

- Image decoders process untrusted byte streams; that is the natural attack
  surface of this extension.
- Keep the extension up to date. Each release pulls in the latest security
  fixes from upstream libraries.
- A future release will add configurable pixel and memory limits before
  decoding (see `docs/ROADMAP.md` and the Limitations + Plan document).
- Run the host application with reduced privileges where possible.
