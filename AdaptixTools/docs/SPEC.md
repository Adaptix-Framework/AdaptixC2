# axtool specification reference

`axtool` reads two kinds of YAML specs:

| File                              | Author              | Purpose                                        |
|-----------------------------------|---------------------|------------------------------------------------|
| **`adaptix.spec`** (project root) | Operator            | Paths, packages, host deps, systemd            |
| **`<package>/axtool.spec`**       | Plugin / kit author | Extenders and/or AxScript kits, build, release |

CLI form for project commands:

```bash
axtool <path-to-adaptix.spec|project-root> <command> …
```

There is no `-s` / `--spec` flag.

---

## 1. Project spec — `adaptix.spec`

Located at the AdaptixC2 repository root (directory that contains `AdaptixServer/` and usually `AdaptixClient/`).

### Fields

| Field             | Required   | Relative to    | Description                                                                                                                                |
|-------------------|------------|----------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| `server_version`  | yes        | —              | Teamserver version string compared to plugin `min_server_version` (e.g. `"v2.0"`)                                                          |
| `server_dir`      | no         | project root   | Teamserver sources + `go.work`. Default: `AdaptixServer`                                                                                   |
| `plugin_dir`      | yes        | **server_dir** | Where plugin sources are cloned/copied. Example: `extenders`                                                                               |
| `ext_dir`         | yes        | project root   | Deployed extender releases. Example: `dist/extenders`                                                                                      |
| `profile`         | no         | project root   | Runtime profile patched by axtool. Default: `<dist_dir>/profile.yaml` or sibling of `ext_dir`                                              |
| `ext_prefix`      | no         | —              | Path prefix for extender entries **as seen from the server CWD**. Default: basename of `ext_dir` (e.g. `extenders`)                        |
| `axscript_dir`    | no         | project root   | AxScript kit install root. Default: sibling of `ext_dir` named `axscripts`                                                                 |
| `axscript_prefix` | no         | —              | Prefix for `Teamserver.axscripts` entries. Default: basename of `axscript_dir`                                                             |
| `client_dir`      | no         | project root   | GUI client sources. Default: `AdaptixClient`                                                                                               |
| `dist_dir`        | no         | project root   | Binary staging directory (`adaptixserver`, `AdaptixClient`, certs, …). Default: `dist`, or parent of `ext_dir` if it ends with `extenders` |
| `repo_root`       | no         | project root   | Optional override; rarely needed when `adaptix.spec` is already at the monorepo root                                                       |
| `packages`        | no         | —              | Default package list for `server build` and `ext install --packages`                                                                       |
| `deps`            | no         | —              | Host apt packages for `--install-deps`                                                                                                     |
| `systemd`         | no         | —              | Defaults for `server daemon`                                                                                                               |

### packages[]

| Field        | Required   | Description                                                                      |
|--------------|------------|----------------------------------------------------------------------------------|
| `source`     | yes        | Local path, `github.com/org/repo@ref`, or git URL                                |
| `path`       | no         | Subdirectory inside the source that contains `axtool.spec`                       |
| `name`       | no         | Install only this extender/script name from a multi-item package                 |
| `plugin_dir` | no         | Override top-level `plugin_dir` for this package only (relative to `server_dir`) |

### deps

Only **apt** is implemented (Debian/Ubuntu). Packages come from the host’s existing apt sources (no extra repos).

```yaml
deps:
  common:
    apt: [git, make, build-essential]
  server:
    apt: [libssl-dev, mingw-w64, g++-mingw-w64]
  client:
    apt: [cmake, qt6-base-dev, qt6-websockets-dev]
```

- `server build -d` installs `common` + `server` (+ apt deps from local `packages:` that already have `axtool.spec`)
- `client build -d` installs `common` + `client`

### systemd

| Field       | Default         | Description                                            |
|-------------|-----------------|--------------------------------------------------------|
| `name`      | `adaptixserver` | Unit basename → `/etc/systemd/system/<name>.service`   |
| `user`      | empty / root    | Process user; empty or `root` omits `User=`            |
| `group`     | empty / root    | Process group; empty or `root` omits `Group=`          |
| `debug`     | `false`         | Append `-debug` to `ExecStart`                         |
| `user_mode` | `false`         | `true` → systemd user unit (`~/.config/systemd/user/`) |

### Example (monorepo root)

```yaml
server_version: "v2.0"

server_dir: AdaptixServer
client_dir: AdaptixClient
plugin_dir: extenders

dist_dir: dist
ext_dir: dist/extenders
axscript_dir: dist/axscripts
profile: dist/profile.yaml

systemd:
  name: adaptix
  user: root

deps:
  common:
    apt: [git, make, build-essential]
  server:
    apt: [libssl-dev, mingw-w64]
  client:
    apt: [cmake, qt6-base-dev]

packages:
  - source: ./AdaptixServer/extenders/beacon_listener_http
  - source: ./AdaptixServer/extenders/beacon_agent
```

### Path semantics

| On install of extender `foo`   | Derived from                                                                               |
|--------------------------------|--------------------------------------------------------------------------------------------|
| Source tree                    | `server_dir` / `plugin_dir` / `foo` (or in-tree path if source already under `server_dir`) |
| Release deploy                 | `ext_dir` / `foo`                                                                          |
| `go.work` use                  | `./<rel-from-server_dir>`                                                                  |
| Profile entry                  | `<ext_prefix>/foo/<config>`                                                                |
| State file                     | `server_dir/.installed_plugins.yaml`                                                       |

`<config>` is the config file path **inside** the release root (see `release.config` / auto-detect below).

---

## 2. Package spec — `axtool.spec`

Lives at the root of a plugin or kit repository (or a monorepo root listing several items). May declare **extenders**, **scripts**, or both.

### Extenders

| Field                | Required  | Description                                                                         |
|----------------------|-----------|-------------------------------------------------------------------------------------|
| `name`               | yes       | Unique id: `[a-z0-9][a-z0-9_-]*`. Directory name under source and release trees     |
| `version`            | yes       | Version string                                                                      |
| `type`               | yes       | `listener` \| `agent` \| `service`                                                  |
| `description`        | no        | Human-readable summary                                                              |
| `author`             | no        | Attribution                                                                         |
| `min_server_version` | no        | Soft check vs project `server_version` (hard-fail unless `--ignore-version`)        |
| `requires`           | no        | Other plugin names; warn if missing from install state                              |
| `source`             | no        | Subdirectory of the package that is this plugin’s root (multi-plugin repos)         |
| `deps.apt`           | no        | Extra apt packages for this plugin (`ext install -d` / merged into server build -d) |
| `build`              | yes       | Ordered shell commands run via `sh -c` in the plugin source directory               |
| `release`            | yes       | Exactly one of `dir` or `globs`                                                     |

### release

| Field    | Required   | Description                                                                                                                                                                          |
|----------|------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `dir`    | one of     | Directory under plugin source; entire contents are deployed to `ext_dir/<name>/`                                                                                                     |
| `globs`  | one of     | Glob patterns relative to plugin source (in-place builds without a `dist/`)                                                                                                          |
| `config` | no         | Config path **relative to the release root** after collect. Written into the profile as `<ext_prefix>/<name>/<config>`. Default: auto-detect shallowest `config.yaml` / `config.yml` |

After build, axtool verifies the release contains:

1. An extender config (`release.config` or auto-detected `config.yaml` / `config.yml`)
2. At least one `*.so` file

### Scripts (AxScript kits)

| Field                                           | Required   | Description                                                        |
|-------------------------------------------------|------------|--------------------------------------------------------------------|
| `name`                                          | yes        | Kit id / install directory name under `axscript_dir`               |
| `version`                                       | yes        | Version string                                                     |
| `entry`                                         | yes        | Main `.axs` path relative to kit root (must end in `.axs`)         |
| `source`                                        | no         | Subdirectory of the package that is the kit root                   |
| `description` / `author` / `min_server_version` | no         | Same semantics as extenders                                        |
| `release`                                       | no         | Optional `dir` or `globs`; default = copy whole tree except `.git` |

Profile registration:

```text
Teamserver.axscripts:
  - <axscript_prefix>/<name>/<entry>
```

### Validation rules

- At least one of `extenders` or `scripts`
- Names unique across extenders and scripts in the same file
- Extender: non-empty `build`; exactly one of `release.dir` / `release.globs`
- Safe relative paths only (no absolute paths, no `..` segments where enforced)

---

## 3. Examples

### 3.1 Single plugin (`release.dir`)

```yaml
extenders:
  - name: beacon_agent
    version: 1.0.0
    type: agent
    min_server_version: "v2.0"
    requires: [beacon_listener_http]
    deps:
      apt: [mingw-w64, g++-mingw-w64]
    build:
      - make
    release:
      dir: dist/
```

### 3.2 In-place plugin (`release.globs`)

```yaml
extenders:
  - name: mcp_server
    version: 1.0.0
    type: service
    build:
      - make
    release:
      globs:
        - config.yaml
        - mcp_server.so
        - ax_config.axs
      # config: config.yaml   # optional override
```

### 3.3 Multi-plugin monorepo

```yaml
extenders:
  - name: my_agent
    version: 1.0.0
    type: agent
    source: ./my_agent
    build: [make]
    release: { dir: dist/ }

  - name: my_listener
    version: 1.0.0
    type: listener
    source: ./my_listener
    build: [make]
    release: { dir: dist/ }
```

Install one item:

```bash
axtool adaptix.spec ext install github.com/org/repo@v1 --name my_agent
```

### 3.4 AxScript kit

```yaml
scripts:
  - name: extension-kit
    version: 1.0.0
    entry: extension-kit.axs
    min_server_version: "v2.0"
```

### 3.5 Packages file (`ext install --from`)

```yaml
packages:
  - source: ./AdaptixServer/extenders/beacon_agent
  - source: github.com/org/plugin@v1
    name: only_this_extender
  - source: github.com/org/monorepo@main
    path: services/foo
```

---

## 4. Install state

File: **`AdaptixServer/.installed_plugins.yaml`** (managed by axtool; do not hand-edit).

Per entry:

| Field                       | Meaning                                                                |
|-----------------------------|------------------------------------------------------------------------|
| `name` / `version` / `type` | Identity (`type` may be `listener`, `agent`, `service`, or `axscript`) |
| `source`                    | Origin URL/path used at install time                                   |
| `commit`                    | Git commit when source was a git clone                                 |
| `source_path`               | Relative to `server_dir`                                               |
| `release_path`              | Relative to `server_dir`                                               |
| `profile_entry`             | Value written under `Teamserver.extenders` or `Teamserver.axscripts`   |
| `installed_at`              | UTC timestamp                                                          |

---

## 5. Related files

| Path                     | Role                                                  |
|--------------------------|-------------------------------------------------------|
| `AdaptixServer/go.work`  | Workspace modules; axtool adds `./plugin_dir/name`    |
| Runtime `profile`        | `Teamserver.extenders` / `Teamserver.axscripts` lists |
| `dist_dir/adaptixserver` | Staged server binary                                  |
| `dist_dir/AdaptixClient` | Staged client binary                                  |
