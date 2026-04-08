# OS/161 on Windows (Docker + WSL2)

A practical OS/161 development setup for Windows AMD64 using Docker Desktop, WSL2 integration, and a bind-mounted workspace for two-way file sync.

## Highlights

- Ubuntu 14.04-based toolchain image (OS/161-compatible GCC/binutils/gdb)
- Docker Compose workflow for repeatable startup
- Live sync between local files and container workspace
- Ready to configure, build, and run OS/161 kernels

## Repository Layout

```text
OS/
  README.md
  OS161_Windows_Setup.md
  OS161/
    Dockerfile
    docker-compose.yml
    os161/
      os161-1.99/
      root/
    os161-binutils/
    os161-gcc/
    os161-gdb/
    os161-bmake/
    os161-mk/
    sys161/
```

## Prerequisites

- Windows 10/11
- Docker Desktop (WSL2 backend enabled)
- WSL2 installed
- Git

## Quick Start

Run from `OS161/`:

```powershell
cd C:\git\OS\OS161
docker compose up -d
docker exec -it os161-dev /bin/bash
```

Inside the container:

```bash
cd /root/cs350-os161/os161-1.99
./configure
cd kern/conf
./config ASST0
cd ../compile/ASST0
bmake depend
bmake
bmake install
cd /root/cs350-os161
ln -sf /root/sys161/share/examples/sys161/sys161.conf.sample sys161.conf
sys161 root/kernel
```

## Two-Way File Sync

`docker-compose.yml` mounts:

- Local: `./os161`
- Container: `/root/cs350-os161`

This means:

- Edit in VS Code on Windows -> reflected in container immediately
- Build/install in container -> output appears locally (including `os161/root/kernel`)

## Run a Host C Program in Container

Create `OS161/os161/hello.c`, then:

```bash
cd /root/cs350-os161
gcc hello.c -o hello
./hello
```

## Common Commands

```powershell
# Start services
cd C:\git\OS\OS161
docker compose up -d

# Open shell
docker exec -it os161-dev /bin/bash

# Stop services
docker compose down
```

## Windows Notes

- Use Windows paths in PowerShell (`C:\...`), not WSL paths (`/mnt/c/...`).
- `docker compose exec` needs both service and command:

```powershell
docker compose exec os161 /bin/bash
```

- `con` is a reserved Windows filename. If Git add fails on:
  `OS161/os161/os161-1.99/man/dev/con.html`
  exclude `man/` (or that file) from staging on Windows.

## Troubleshooting

- `Cannot open config file sys161.conf`:
  create symlink in `/root/cs350-os161` as shown above.
- `docker compose build` says no config file:
  run from `OS161/` or pass `-f OS161/docker-compose.yml`.
- If Docker Hub DNS fails (`auth.docker.io`):
  restart Docker Desktop/WSL and check DNS/proxy/VPN settings.

## Status

This setup is suitable for local lab development and testing. For grading, still validate behavior in your course's official environment.
