# PaneBind

PaneBind is an early-stage, open-source desktop window-enhancement project.
Windows is the first implementation platform, while the geometry and event
model are designed to remain platform-neutral.

The repository is currently in **R0: research and architecture baseline**. R0
contains a read-only Windows observer and pure geometry tests; it deliberately
does not implement snapping, glued movement/resizing, or control of third-party
windows.

## Build

Requirements:

- CMake 3.25 or newer
- a C++20 compiler
- Windows SDK for the observer target

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

On Windows, the resulting `panebind-observer.exe` emits JSON Lines.
Run it with `--enumerate-only` for a one-time snapshot or without arguments to
observe the three R0 WinEvent types until interrupted.

See [the project charter](docs/charter/PROJECT_CHARTER.md),
[architecture](docs/architecture/ARCHITECTURE.md), and
[R0 research](docs/research/PRIOR_ART_REVIEW.md) for scope and evidence.

## License

PaneBind is licensed under the MIT License. External research sources and
their code-use status are tracked separately in
`docs/research/SOURCE_PROVENANCE.md`.
