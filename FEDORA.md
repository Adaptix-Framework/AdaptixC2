# Building AdaptixC2 on Fedora

The upstream host installer targets Debian-family distributions and uses
`apt`. On Fedora, use the repository's container build through Podman instead.
This keeps the required Go, MinGW, CMake, and Qt versions isolated from the
host and does not require Docker Compose.

## Prerequisite

```bash
sudo dnf install podman
```

For rootless Podman, log out and back in after a fresh installation if the
user namespace configuration was changed by the package installation.

## Build

From the repository root:

```bash
# Qt client packaged as an AppImage
make fedora-client

# Teamserver and all standard extenders
make fedora-server-ext

# Both
make fedora-all
```

## Easy install and launch

Build and create a self-contained local installation:

```bash
./install-fedora.sh
```

The installer creates a local-only profile, unique password, TLS certificate,
and `connection.txt` under `./install`. Existing profile credentials and data
are preserved when the installer is run again.

Before the first login, display the generated connection details:

```bash
cat install/connection.txt
```

The generated profile uses `only_password: true`. Enter any operator nickname
in the client and use the generated password shown in `connection.txt`. The
upstream example credentials such as `operator1` / `pass1` are inactive in
this mode, and the default `pass` is replaced. Re-running the installer
preserves the generated password.

If a built `Extension-Kit` checkout exists in the repository root, the
installer also copies its `.axs` scripts and `_bin` runtime artifacts into the
installation and enables `Extension-Kit/extension-kit.axs` in the server
profile. Set `ADAPTIX_EXTENSION_KIT_DIR=/path/to/Extension-Kit` when the kit is
stored elsewhere. The Extension Kit repository itself is not added to this
repository or committed by the installer.

Start the server and client together:

```bash
./run-adaptix.sh
```

The server is stopped automatically when the client closes. The installed
copy can also be launched directly:

```bash
./install/run-adaptix.sh
```

Additional launcher modes are available:

```bash
./run-adaptix.sh server
./run-adaptix.sh client
./run-adaptix.sh status
./run-adaptix.sh stop
```

Use another server profile with the same `-profile` argument accepted by
`adaptixserver`:

```bash
./run-adaptix.sh server -profile /path/to/profile.yaml
./run-adaptix.sh both -profile profile-lab.yaml
```

Relative profile paths are resolved strictly from the current directory. A
missing explicit path is reported as an error and never falls back to the
generated profile. To select that profile explicitly from the repository root,
use `-profile install/server/profile.yaml`. The selected profile is also used
for the launcher's server readiness check.

Outputs are written to:

- `AdaptixClient/client-dist/AdaptixClient-x86_64.AppImage`
- `AdaptixServer/server-dist/`

The wrapper can also be invoked directly:

```bash
scripts/build-fedora.sh client
scripts/build-fedora.sh server
scripts/build-fedora.sh extenders
scripts/build-fedora.sh server-ext
scripts/build-fedora.sh all
```

Set `CONTAINER_ENGINE=docker` to use Docker instead. The Podman path includes
the SELinux volume label and rootless user mapping needed on a standard Fedora
installation.

## Run the client

```bash
chmod +x AdaptixClient/client-dist/AdaptixClient-x86_64.AppImage
QT_QPA_PLATFORM=xcb AdaptixClient/client-dist/AdaptixClient-x86_64.AppImage
```

The package uses XCB through Fedora's XWayland compatibility layer. Setting
`QT_QPA_PLATFORM=xcb` avoids a harmless warning about the Wayland Qt plugin.

If FUSE is unavailable, AppImage supports extraction:

```bash
AdaptixClient/client-dist/AdaptixClient-x86_64.AppImage --appimage-extract
QT_QPA_PLATFORM=xcb ./squashfs-root/AppRun
```
