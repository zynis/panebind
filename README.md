# PaneBind

PaneBind is an early-stage, open-source desktop window-enhancement project.
Windows is the first implementation platform, while the geometry and event
model are designed to remain platform-neutral.

The current R1-C3B baseline includes a narrowly authorized two-Explorer
Ctrl+Move test session, with human Debug smoothness accepted between A and B.
The [human validation report](docs/reports/R1C3B_HUMAN_VALIDATION_REPORT.md)
records its evidence, authority boundaries and limitations. This is not a
general-purpose or production-ready window manager. R0 remains a separate
read-only observer; Glue Resize and Snap integration are not implemented.

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
