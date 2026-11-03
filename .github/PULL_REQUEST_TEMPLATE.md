## Summary

<!-- What does this PR change, and why? Link the issue: Closes #N -->

## Workstream / scope

- [ ] DSP (`src/dsp/`, `tests/`)
- [ ] UI (`src/ui/`)
- [ ] Docs (`docs/`, `README.md`)
- [ ] Presets (`presets/`, `tools/DemoScore.*`)
- [ ] Infra / QA

## Evidence

<!-- Paste exact commands + observed output. Screenshots for UI changes. -->

```
./scripts/build.sh <target>
./build/nagl_tests_artefacts/Release/nagl_tests
```

## Realtime-safety checklist (DSP changes)

- [ ] No allocation / locks / file I/O in `process()`, `setParams()` or `getVisual()`
- [ ] All parameter changes smoothed (no zipper noise)
- [ ] Denormal / NaN guarded; output finite under extreme settings
- [ ] Mono and stereo layouts tested; block sizes 128/512/4096 tested
- [ ] CPU measured and reported at 48 kHz / 128 samples

## Reviewer notes

<!-- Anything the reviewer should scrutinise; known limitations. -->
