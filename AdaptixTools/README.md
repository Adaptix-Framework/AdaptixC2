# axtool — AdaptixC2 operator toolkit

`axtool` builds the teamserver and GUI client, installs extenders / AxScript kits, patches the runtime profile, and manages a systemd unit.

Project layout is described by **`adaptix.spec`** at the AdaptixC2 repository root. Package plugins and kits use **`axtool.spec`**.

There is **no** `-s` / `--spec` flag. For project commands the path is always the first argument:

```bash
axtool <adaptix.spec|project-root> <command> …
```

Commands that do not need a project (`template`, `completion`) omit that argument.

---

## Build

```bash
# From AdaptixTools/
make
# → dist/axtool

# Or from the monorepo root
make tools
# → dist/axtool
```

Requires **Go 1.26+**.

---

## Quick start (monorepo)

```bash
# Build teamserver into dist/, install packages: from adaptix.spec, seed TLS certs
axtool adaptix.spec server build --gen-cert -d

# Build GUI client into dist/
axtool adaptix.spec client build

# Install systemd unit and start (needs root for a system unit)
axtool adaptix.spec server daemon install --start

# List installed plugins
axtool adaptix.spec ext list

# Install one local plugin (force rebuild)
axtool adaptix.spec ext install ./AdaptixServer/extenders/beacon_agent -f

# Patch runtime profile scalars
axtool adaptix.spec profile set port=8443 interface=0.0.0.0
```

Typical runtime layout after a successful build:

```text
dist/
  adaptixserver
  AdaptixClient
  profile.yaml
  server.rsa.key / server.rsa.crt   # with --gen-cert
  ssl_gen.sh, 404page.html
  extenders/<plugin>/               # .so + config
  axscripts/<kit>/                  # optional kits
```

Start the server with CWD = `dist_dir`:

```bash
cd dist && ./adaptixserver -profile profile.yaml
```

---

## Command map

| Group          | Commands                                                                                       | Needs `spec` |
|----------------|------------------------------------------------------------------------------------------------|--------------|
| **server**     | `build`, `daemon` (`install` / `uninstall` / `start` / `stop` / `restart` / `status` / `logs`) | yes          |
| **client**     | `build`                                                                                        | yes          |
| **ext**        | `list` (`ls`), `info`, `install`, `uninstall`                                                  | yes          |
| **profile**    | `show`, `get`, `set`                                                                           | yes          |
| **template**   | scaffold `agent` / `listener` / `service` / `axscript`                                         | no           |
| **completion** | shell completion                                                                               | no           |

### server build

```bash
axtool adaptix.spec server build [flags]
```

| Flag                   | Meaning                                                     |
|------------------------|-------------------------------------------------------------|
| `-d`, `--install-deps` | Install apt packages from `deps:` (+ local package `deps:`) |
| `--gen-cert`           | Create `server.rsa.key` / `server.rsa.crt` in `dist_dir`    |
| `--force-cert`         | Overwrite existing certs                                    |
| `--no-packages`        | Do not install `packages:` after the binary build           |
| `--no-profile`         | Do not seed / prune / register profile entries              |

Default flow: compile `adaptixserver` → stage into `dist_dir` → install `packages:` with force → prune missing extender paths from the runtime profile.

### client build

```bash
axtool adaptix.spec client build [-j N] [-d]
```

Stages `AdaptixClient` into **`dist_dir`** from `adaptix.spec` (not a hard-coded path).

### ext install

```bash
axtool adaptix.spec ext install <source> [flags]
axtool adaptix.spec ext install --packages [-f]
axtool adaptix.spec ext install --from packages.yaml
```

| Flag                   | Meaning                                                        |
|------------------------|----------------------------------------------------------------|
| `--name`               | Install only this name from a multi-item package `axtool.spec` |
| `--path`               | Subdirectory of the package that holds `axtool.spec`           |
| `-f`, `--force`        | Reinstall / overwrite                                          |
| `--ignore-version`     | Allow `min_server_version` mismatches                          |
| `-d`, `--install-deps` | Install apt packages from the package `deps:` before build     |
| `--packages`           | Install all `packages:` from `adaptix.spec`                    |
| `--from`               | Install all sources listed in a packages YAML file             |

`<source>` may be a local path, `github.com/org/repo@ref`, or a git URL.

Install steps for an **extender**: copy sources under `plugin_dir`, update `go.work`, run `build:`, collect `release:`, deploy to `ext_dir/<name>/`, register config path under `Teamserver.extenders`, update `AdaptixServer/.installed_plugins.yaml`.

Install steps for an **AxScript kit**: copy into `axscript_dir/<name>/`, register `Teamserver.axscripts`.

### ext uninstall

```bash
axtool adaptix.spec ext uninstall <name> [-c|--clear-source]
```

Removes release tree, profile entry, `go.work` use line, and state. Source tree is kept unless `--clear-source`.

### profile

Operates on the runtime profile path from `profile:` (default derived from `dist_dir`).

| Command                   | Description                           |
|---------------------------|---------------------------------------|
| `profile show`            | List known `Teamserver` scalar fields |
| `profile get <key>`       | Print one field                       |
| `profile set key=value …` | Set one or more fields                |

Editable keys: `interface`, `port`, `endpoint`, `password`, `manage_password`, `only_password`, `cert`, `key`, `access_token_live_hours`, `refresh_token_live_hours`.

### server daemon

Uses `systemd:` from `adaptix.spec`. Working directory = `dist_dir`, binary = `dist_dir/adaptixserver`.

```bash
axtool adaptix.spec server daemon install [--start] [--debug] [--user] …
axtool adaptix.spec server daemon logs -f
```

### template

```bash
axtool template agent|listener|service|axscript [name] [--from <git-or-path>] [--protocol <listener-protocol>]
```

Scaffolds from [templates-extender](https://github.com/Adaptix-Framework/templates-extender) (or a local/git override). Does not require `adaptix.spec`.

---

## Spec files (summary)

| File               | Role                                                     |
|--------------------|----------------------------------------------------------|
| **`adaptix.spec`** | Project: paths, packages, deps, systemd                  |
| **`axtool.spec`**  | Package: extenders and/or AxScript kits, build + release |

Full field reference: [`docs/SPEC.md`](docs/SPEC.md).  
YAML samples: [`docs/examples/`](docs/examples/).

---

## Layout notes

- Paths in `adaptix.spec` are relative to the directory that contains the file (project root), except **`plugin_dir`**, which is relative to **`server_dir`**.
- Extender release paths in `profile.yaml` are relative to the server working directory (usually `dist_dir`). Formed as `<ext_prefix>/<name>/<config>` where `config` comes from the release (`release.config` or auto-detected `config.yaml` / `config.yml`).
- Install state: `AdaptixServer/.installed_plugins.yaml` (gitignored).
- Client and server binaries are staged to **`dist_dir`**, not hard-coded `"dist"`.

---

## Troubleshooting

| Symptom                                     | What to check                                                                                             |
|---------------------------------------------|-----------------------------------------------------------------------------------------------------------|
| `adaptix.spec is required`                  | Pass the project path first: `axtool adaptix.spec server build` (no `-s` flag)                            |
| `release does not contain a config file`    | Makefile must ship config into `release.dir`, or list it in `globs`; non-standard name → `release.config` |
| `release does not contain any .so file`     | Build output path vs `release.dir` / globs                                                                |
| Plugin missing at runtime                   | Server CWD is usually `dist_dir`; profile entry must exist as `<ext_prefix>/<name>/<config>`              |
| Stale / invalid profile after partial build | `axtool adaptix.spec server build --no-packages` still prunes missing extender paths                      |
| Client binary not found                     | `client_dir` must build `AdaptixClient` (or `build/AdaptixClient`); staged to `dist_dir`                  |
| systemd install fails                       | System units need root, or use `systemd.user_mode: true` / `--user`                                       |

Useful resets:

```bash
axtool adaptix.spec ext install ./path/to/plugin -f
axtool adaptix.spec ext uninstall plugin_name -c   # also drop source under plugin_dir
```
