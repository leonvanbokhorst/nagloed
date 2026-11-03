# Contributing to NAGLOED

NAGLOED uses a **gitflow** workflow with independent review. Every change reaches
`main` through a reviewed pull request; nothing is pushed to `main` directly.

## Branches

| Branch | Purpose |
|---|---|
| `main` | Released, tagged, always green. Only updated from `release/*` or `hotfix/*` PRs. |
| `develop` | Integration branch. Feature PRs target this. |
| `feature/<scope>` | One workstream / issue. Branch from `develop`, PR back to `develop`. |
| `release/<x.y.z>` | Stabilisation: version bump, release notes. PR to `main`, then merge back to `develop`. |
| `hotfix/<scope>` | Emergency fix. PR to `develop` (or `main` if a release is live), then merge back. |

Branch names mirror the issue: `feature/dsp-engine` closes `#1`.

## Worktrees

Parallel work uses git worktrees so nobody shares a checkout:

```bash
git worktree add .worktrees/<name> -b feature/<scope> develop
ln -s ../../.deps .worktrees/<name>/.deps
```

## Commits

Conventional-commit prefixes: `feat:`, `fix:`, `perf:`, `test:`, `docs:`,
`refactor:`, `chore:`. Keep commits reviewable; explain *why* in the body when
the change is non-obvious.

## Pull requests

1. Fill in the PR template, including exact test/validation commands and output.
2. UI changes include a rendered screenshot (`docs/ui-preview.png` via `nagl-preview`).
3. DSP changes include CPU numbers and the realtime-safety checklist.
4. An **independent reviewer** (a separate agent/engineer, never the author)
   reviews the diff, runs the build and tests, and either approves or requests
   changes. Review comments are addressed with follow-up commits.
   *Repository note:* NAGLOED currently has a single GitHub maintainer account,
   so GitHub refuses formal approvals on the maintainer's own PRs. Reviews are
   therefore recorded as `COMMENTED` reviews with an explicit verdict plus the
   `reviewed` label; the verdict in the review body is the gate, not
   `reviewDecision`. When a second maintainer account exists this becomes a
   normal `APPROVE`.
5. Merge with **squash merge** into `develop`; the PR title becomes the commit.

## Local checks

```bash
./scripts/fetch-juce.sh      # once
./scripts/build.sh           # plugin + tools
./scripts/validate.sh        # tests + auval (+ pluginval if installed)
```

## Reviewing a PR

```bash
gh pr diff <n>
git worktree add .worktrees/review-<n> -B review/<n> origin/<branch>
ln -s ../../.deps .worktrees/review-<n>/.deps
# build + run the relevant tests in that worktree, then:
gh pr review <n> --approve   # or --request-changes / --comment
git worktree remove --force .worktrees/review-<n>
```

## Releasing

```bash
git checkout -b release/1.0.0 develop
# bump project(NAGLOED VERSION ...) in CMakeLists.txt, finalise docs/RELEASE_NOTES.md
git commit -am "chore(release): 1.0.0"
gh pr create --base main --title "release: 1.0.0"
# after review + merge, tag the merge commit that is actually on main:
git checkout main && git pull
git tag -a v1.0.0 -m "NAGLOED 1.0.0" && git push origin v1.0.0
gh release create v1.0.0 --generate-notes
```

## Code standards

- C++20, JUCE 9, 4-space indent, 100-col soft wrap, `camelCase` methods.
- Audio thread: no allocation, locks, exceptions, logging or file I/O.
- Comments explain musical intent, not just mechanics.
- No image/font assets: the UI is drawn procedurally.
- macOS is the supported build platform for 1.0 (AU, VST3, Standalone).
