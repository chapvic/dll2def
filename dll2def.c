#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

static void* realloc_ex(void* ptr, unsigned long size) {
    void* p = realloc(ptr, size);
    if (!p && size) {
        puts("dll2def: not enough memory!");
        exit(1);
    }
    return p;
}

static char* basename(char* path) {
    char* p = strchr(path, 0);
    while (p > path && !(p[-1] == '/' || p[-1] == '\\')) p--;
    return p;
}

static int _readfile(int handle, unsigned offset, void* buffer, unsigned len) {
    _lseek(handle, offset, SEEK_SET);
    return (len == _read(handle, buffer, len));
}


// ---------------------------
// Get exported names from DLL
// ---------------------------
// Returns:
// 0 - success
// 1 - file not found
// 2 - invalid file format
// 3 - no symbols found
static int dll_get_exports(char* filename, char** pp) {
    IMAGE_SECTION_HEADER SectionHeader;
    IMAGE_EXPORT_DIRECTORY ExportDirectory;
    IMAGE_DOS_HEADER DosHeader;
    IMAGE_FILE_HEADER FileHeader;
    DWORD sign, ref, v_addr, ptr, name_ptr;
    char* p = NULL;
    int peOffset, optOffset, secOffset,
        fn = -1,    // initial invalid file handle
        ret = 1;    // initial file not found error

    fn = _open(filename, O_RDONLY | O_BINARY);
    if (fn >= 0) {
        ret = 2;
        if (_readfile(fn, 0, &DosHeader, sizeof(DosHeader)) &&
            _readfile(fn, DosHeader.e_lfanew, &sign, sizeof(sign)) &&
            sign == IMAGE_NT_SIGNATURE /*0x00004550*/) {

            peOffset = DosHeader.e_lfanew + sizeof(sign);
            if (_readfile(fn, peOffset, &FileHeader, sizeof(FileHeader))) {
                optOffset = peOffset + sizeof(FileHeader);
                if (FileHeader.Machine == IMAGE_FILE_MACHINE_I386 /*0x014C*/) {
                    IMAGE_OPTIONAL_HEADER32 OptionalHeader;
                    secOffset = optOffset + sizeof(OptionalHeader);
                    if (_readfile(fn, optOffset, &OptionalHeader, sizeof(OptionalHeader))) {
                        if (OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
                            ret = 3; goto cleanup;
                        }
                        v_addr = OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    }
                    else goto cleanup;
                }
                else if (FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 /*0x8664*/) {
                    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
                    secOffset = optOffset + sizeof(OptionalHeader);
                    if (_readfile(fn, optOffset, &OptionalHeader, sizeof(OptionalHeader))) {
                        if (OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) {
                            ret = 3; goto cleanup;
                        }
                        v_addr = OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    }
                    else goto cleanup;
                }
                else goto cleanup;

                ret = 3;
                for (int i = 0; i < FileHeader.NumberOfSections; ++i) {
                    if (_readfile(fn, secOffset + i * sizeof(SectionHeader), &SectionHeader, sizeof(SectionHeader))) {
                        if (v_addr >= SectionHeader.VirtualAddress && v_addr < SectionHeader.VirtualAddress + SectionHeader.SizeOfRawData) {
                            ref = SectionHeader.VirtualAddress - SectionHeader.PointerToRawData;
                            if (_readfile(fn, v_addr - ref, &ExportDirectory, sizeof(ExportDirectory))) {
                                name_ptr = ExportDirectory.AddressOfNames - ref;
                                    int n = 0, n0 = 0;
                                    for (unsigned i = 0; i < ExportDirectory.NumberOfNames; ++i) {
                                        if (_readfile(fn, name_ptr, &ptr, sizeof(ptr))) {
                                            name_ptr += sizeof(ptr);
                                            int l = 0;
                                            while (1) {
                                                if (n + 1 >= n0)
                                                    p = realloc_ex(p, n0 = n0 ? n0 * 2 : 256);
                                                if (!_readfile(fn, ptr - ref + l++, p + n, 1)) {
                                                    free(p);
                                                    p = NULL;
                                                    goto cleanup;
                                                }
                                                if (p[n++] == 0)
                                                    break;
                                            }
                                        }
                                    }
                                if (p) p[n] = 0;
                                ret = 0;
                            }
                        }
                    }
                }
            }
        }
    }

cleanup:
    if (fn >= 0) _close(fn);
    *pp = p;
    return ret;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        puts("dll2def - Create .def file from dynamic link library");
        puts("Copyright (c) 2020, FoxTeam\n");
        puts("Usage: dll2def <infile> [outpath]");
        return 0;
    }

    char infile[MAX_PATH] = { 0 };
    char outfile[MAX_PATH] = { 0 };
    char* p = NULL;
    int i;

    SearchPath(NULL, argv[1], ".dll", sizeof(infile), infile, NULL);

    int ret = dll_get_exports(infile, &p);
    if (ret || !p) {
        fprintf(stdout, "dll2def: %s '%s'\n",
            ret == 1 ? "can't find file" :
            ret == 2 ? "invalid file format" :
            "no symbols found in", argv[1]);
        return 1;
    }

    char* q;
    if (argc > 2) {
        strcpy(outfile, argv[2]);
        q = strrchr(outfile, '\\');
        if (!q) q = strrchr(outfile, '/');
        if (q && q[1] == 0) strcat(outfile, basename(infile));
    }
    else {
        strcpy(outfile, basename(infile));
    }

    q = strrchr(outfile, '.');
    if (!q) q = strchr(outfile, 0);
    strcpy(q, ".def");

    FILE* def = fopen(outfile, "wb");
    if (!def) {
        fprintf(stdout, "dll2def: could not create output file: %s\n", outfile);
        return 1;
    }
    fprintf(def, "LIBRARY %s\n\nEXPORTS\n", basename(infile));
    for (q = p, i = 0; *q; ++i) {
        fprintf(def, "%s\n", q);
        q += strlen(q) + 1;
    }
    fclose(def);

    return 0;
}

