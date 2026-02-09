# Git Submodules

This project uses Git submodules to manage third-party dependencies.

## Submodules

- `third_party/cpp-httplib` - Header-only HTTP server library
- `third_party/nlohmann-json` - JSON parsing library

## Initial Setup

After cloning the repository, initialize the submodules:

```bash
git submodule update --init --recursive
```

## Updating Submodules

To update all submodules to their latest compatible versions:

```bash
git submodule update --remote --merge
git add third_party/cpp-httplib third_party/nlohmann-json
git commit -m "Update submodules"
```

## Specific Versions

The project is tested with these specific versions:
- cpp-httplib: v0.31.0
- nlohmann/json: v3.11.2

When updating, test compatibility before committing.
