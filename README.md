# openat2-resolve

An experiment to try out the different resolve flags for the syscall [`openat2()`](https://man.archlinux.org/man/openat2.2.en)

### Requirements

* linux 5.6 or later. (linux 5.6 introduced the system call `openat2()`)
* git
* podman (or some other container engine like docker or nerdctl)

### Installation

1. Clone repo
   ```
   git clone https://github.com/eriksjolund/openat2-resolve.git
   ```
2. Build the container image
   ```
   podman build -t builder openat2-resolve/
   ```
3. Create build directory
   ```
   mkdir build
   ```
4. Build the executable `./build/openat2-resolve`
   ```
   podman run --rm -ti -v ./build:/build:Z -v ./openat2-resolve:/src:Z -w /build localhost/builder bash -c "cmake ../src/ && cmake --build ."
   ```

### Usage

```
./build/openat2-resolve dirpath [RESOLVE_FLAG...]
```
