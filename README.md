# dll2def - Pure C win32 application to create .def file from a dynamic link library (DLL)

### Compile:

_GCC_
```
gcc -O2 -D_CRT_SECURE_NO_WARNINGS -s -static -o dll2def.exe dll2def.c
```
_MSVC_
```
cl /MT /O2 /D_CRT_SECURE_NO_WARNINGS dll2def.c
```

### Usage:

```
dll2def <infile> [outpath]
  infile   - input file
  outpath  - full or relative output path
```

### Examples:

```
1. dll2def shell32.dll            - create shell32.def in the current directory
2. dll2def shell32                - same as (1), but '.dll' extension will be added automaticaly
3. dll2def shell32 \some\path\    - create \some\path\shell32.def
4. dll2def shell32 \some\path\my  - create \some\path\my.def, extension will be added automaticaly
```
