# OS/161 Installation on Windows (AMD64)

## Overview
These instructions adapt the OS/161 setup for Windows AMD64 systems. You have two main options:

### Option 1: Native Windows (Recommended for AMD64)
Use Windows Subsystem for Linux (WSL2) with Ubuntu - this gives you a native Linux environment on Windows while leveraging your AMD64 hardware.

### Option 2: Virtual Machine
Use VirtualBox or Hyper-V with a Linux VM (less efficient on Windows but fully isolated).

---

## **Option 1: Windows Subsystem for Linux 2 (WSL2) - RECOMMENDED**

### Prerequisites
- Windows 10 (version 1903+) or Windows 11
- AMD64 processor with virtualization enabled in BIOS
- ~15GB free disk space

### Step 1: Install WSL2

**PowerShell (run as Administrator):**
```powershell
wsl --install
wsl --set-default-version 2
```

This installs WSL2 and Ubuntu by default. Restart your computer when prompted.

### Step 2: Launch Ubuntu and Update

```bash
# Open Ubuntu from Start Menu or run: ubuntu
sudo apt update
sudo apt upgrade -y
```

### Step 3: Install Build Dependencies

```bash
sudo apt install -y build-essential texinfo libncurses-dev bison flex
```

### Step 4: Download OS/161 Files

Create a working directory:
```bash
mkdir -p ~/os161-workspace
cd ~/os161-workspace
```

Download all required files:
```bash
# Binutils, GCC, GDB, bmake, mk, sys161, OS/161
# Replace with actual download URLs or use wget:
wget <binutils-url>
wget <gcc-url>
wget <gdb-url>
wget <bmake-url>
wget <mk-url>
wget <sys161-url>
wget <os161-url>
```

Or download from your browser and copy into WSL using:
```bash
# From Windows File Explorer:
# Navigate to: \\wsl$\Ubuntu\home\<username>\os161-workspace
# Copy downloaded .tar.gz files here
```

### Step 5: Build Binutils

```bash
cd ~/os161-workspace
tar -xzf os161-binutils.tar.gz
cd binutils-2.17+os161-2.0.1

./configure --nfp --disable-werror --target=mips-harvard-os161 --prefix=$HOME/sys161/tools

make
# If makeinfo error occurs:
find . -name '*.info' | xargs touch
make

make install
```

### Step 6: Setup PATH

Edit `~/.bashrc`:
```bash
nano ~/.bashrc
```

Add at the end:
```bash
export PATH=$HOME/sys161/bin:$HOME/sys161/tools/bin:$PATH
```

Save (Ctrl+O, Enter, Ctrl+X) and reload:
```bash
source ~/.bashrc
```

### Step 7: Build GCC

```bash
cd ~/os161-workspace
tar -xzf os161-gcc.tar.gz
cd gcc-4.1.2+os161-2.0

./configure -nfp --disable-shared --disable-threads --disable-libmudflap --disable-libssp --target=mips-harvard-os161 --prefix=$HOME/sys161/tools

make
make install
```

### Step 8: Build GDB

```bash
cd ~/os161-workspace
tar -xzf os161-gdb.tar.gz
cd gdb-6.6+os161-2.0

./configure --target=mips-harvard-os161 --prefix=$HOME/sys161/tools --disable-werror

make
make install
```

### Step 9: Install bmake

```bash
cd ~/os161-workspace
tar -xzf os161-bmake.tar.gz
cd bmake

tar -xzf ../os161-mk.tar.gz

./boot-strap --prefix=$HOME/sys161/tools

# Run the commands printed by boot-strap script
# They will look like:
# mkdir -p $HOME/sys161/tools/bin
# cp ... (copy bmake binary)
# ln -s ... (create symlinks)
# etc.
```

### Step 10: Setup Toolchain Links

```bash
mkdir -p $HOME/sys161/bin
cd $HOME/sys161/tools/bin

for i in mips-*; do ln -s $HOME/sys161/tools/bin/$i $HOME/sys161/bin/cs350-`echo $i | cut -d- -f4-`; done
ln -s $HOME/sys161/tools/bin/bmake $HOME/sys161/bin/bmake
```

Verify:
```bash
ls -la ~/sys161/bin
# Should show: bmake, cs350-gcc, cs350-gdb, cs350-ld, etc.
```

### Step 11: Build sys161 Simulator

```bash
cd ~/os161-workspace
tar -xzf sys161.tar.gz
cd sys161-1.99.06

./configure --prefix=$HOME/sys161 mipseb

make
make install

# Setup config symlink
cd $HOME/sys161
ln -s share/examples/sys161/sys161.conf.sample sys161.conf
```

### Step 12: Install OS/161

```bash
cd ~
mkdir cs350-os161
cd cs350-os161

mv ~/os161-workspace/os161.tar.gz .
tar -xzf os161.tar.gz

# Now you have os161-1.99 directory ready for development
```

### Step 13: Build and Test OS/161

```bash
cd ~/cs350-os161/os161-1.99

./configure --ostree=$HOME/cs350-os161/install

bmake
bmake install

# Run test kernel
cd ~/cs350-os161/install
sys161 kernel
```

---

## **Option 2: VirtualBox Virtual Machine**

If you prefer isolation or WSL2 causes issues:

1. Download VirtualBox from https://www.virtualbox.org/
2. Create Ubuntu 20.04 LTS VM with:
   - 2+ CPU cores
   - 4GB+ RAM
   - 20GB disk space
3. Follow the **Linux instructions above** (Steps 1-13) inside the VM

---

## Accessing Your Files from Windows

**WSL2 integrates seamlessly:**
- In Windows Explorer: `\\wsl$\Ubuntu\home\<username>\cs350-os161`
- In VS Code: Install "Remote - WSL" extension, open WSL folder

**Editing from Windows:**
- Use Windows VS Code with WSL extension
- Or edit directly in Ubuntu terminal with `nano` or `vim`

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `make: texinfo not found` | `sudo apt install texinfo` |
| `undefined reference to libncurses` | `sudo apt install libncurses-dev` |
| Path not updating | Restart Ubuntu terminal or run `source ~/.bashrc` |
| WSL2 not available | Enable virtualization in BIOS; update Windows |
| Building takes forever | Increase WSL2 CPU/memory in `.wslconfig` |

---

## Next Steps

1. Navigate to `~/cs350-os161/os161-1.99`
2. Follow your course's OS/161 assignment instructions
3. Compile and test kernels using `bmake` and `sys161`

For marking purposes, verify your code compiles in the **student.cs environment** as required by your course.
