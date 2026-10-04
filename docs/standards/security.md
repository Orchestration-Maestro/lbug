# Security rules in `lbug`

`lbug` follows the organization's [security
rules](https://github.com/Orchestration-Maestro/.github/blob/864d85597a833864cd8506c3925830503b3c2163/golden-rules/security.md).
This page is its rule map (C-001): for every rule, what holds it here, or why it
does not apply. A row may name a stricter local rule; none weakens one.

`rust-gate rules` writes the rows from the golden rules of `.github@864d855` at
every commit and keeps what each row says here. A rule added there arrives as
"Not mapped yet", and the daily drift check reports it until it is mapped.

## What this repository protects

Not mapped yet: what this repository holds or runs that an attacker would want,
and where untrusted input enters it.

## Rule map

| Rule | Held here by |
| --- | --- |
| SEC-001 Minimise sensitive data | Not mapped yet |
| SEC-002 Treat input as data | Not mapped yet |
| SEC-003 Validate boundaries | Not mapped yet |
| SEC-004 Use real authority | Organization: rulesets, workflow permissions and the organization bot's own App identity; automation borrows no person's credentials |
| SEC-005 Scope sensitive approvals | Review: a publication, release or settings change is approved in its own pull request |
| SEC-006 Inspect code safely | Not mapped yet |
| SEC-007 Stop and escalate incidents | Review: a suspected exposure stops the work and goes to SECURITY.md's private channel; a leaked secret is revoked and rotated |
| SEC-008 Keep truthful evidence | Review: results are reported as run, with what was not checked |
| SEC-009 Preserve safe progress | Review: blocked work is reported as partial, never as done |
| SEC-010 Report vulnerabilities privately | Organization: private vulnerability reporting is on (`maestrolabs-baseline`), and SECURITY.md routes reports to it |
| SEC-011 Sign every release | Not mapped yet |
