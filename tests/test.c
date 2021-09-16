#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS
#endif
#ifdef __gnu_linux__
#define _GNU_SOURCE
#endif

#include "../srpmalloc.h"
extern int
test_run(int argc, char** argv);

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define pointer_offset(ptr, ofs) (void*)((char*)(ptr) + (ptrdiff_t)(ofs))
#define pointer_diff(first, second) (ptrdiff_t)((const char*)(first) - (const char*)(second))

static int _test_failed;

static int
test_fail_cb(const char* reason, const char* file, int line) {
    fprintf(stderr, "FAIL: %s @ %s:%d\n", reason, file, line);
    fflush(stderr);
    _test_failed = 1;
    return -1;
}

#define test_fail(msg) test_fail_cb(msg, __FILE__, __LINE__)

static int
test_alloc(void) {
    unsigned int iloop = 0;
    unsigned int ipass = 0;
    unsigned int icheck = 0;
    unsigned int id = 0;
    void* addr[8142];
    char data[20000];
    unsigned int datasize[7] = { 473, 39, 195, 24, 73, 376, 245 };
    size_t wanted_usable_size;

    rpmalloc_initialize();

    //Query the small granularity
    void* zero_alloc = rpmalloc(0);
    size_t small_granularity = rpmalloc_usable_size(zero_alloc);
    rpfree(zero_alloc);

    for (id = 0; id < 20000; ++id)
        data[id] = (char)(id % 139 + id % 17);

    //Verify that blocks are 16 byte size aligned
    void* testptr = rpmalloc(16);
    if (rpmalloc_usable_size(testptr) != 16)
        return test_fail("Bad base alloc usable size");
    rpfree(testptr);
    testptr = rpmalloc(32);
    if (rpmalloc_usable_size(testptr) != 32)
        return test_fail("Bad base alloc usable size");
    rpfree(testptr);
    testptr = rpmalloc(128);
    if (rpmalloc_usable_size(testptr) != 128)
        return test_fail("Bad base alloc usable size");
    rpfree(testptr);
    for (iloop = 0; iloop <= 1024; ++iloop) {
        testptr = rpmalloc(iloop);
        wanted_usable_size = iloop ? small_granularity * ((iloop + (small_granularity - 1)) / small_granularity) : small_granularity;
        if (rpmalloc_usable_size(testptr) != wanted_usable_size) {
            printf("For %u wanted %zu got %zu\n", iloop, wanted_usable_size, rpmalloc_usable_size(testptr));
            return test_fail("Bad base alloc usable size");
        }
        rpfree(testptr);
    }

    //Verify medium block sizes (until class merging kicks in)
    for (iloop = 1025; iloop <= 6000; ++iloop) {
        testptr = rpmalloc(iloop);
        wanted_usable_size = 512 * ((iloop / 512) + ((iloop % 512) ? 1 : 0));
        if (rpmalloc_usable_size(testptr) != wanted_usable_size)
            return test_fail("Bad medium alloc usable size");
        rpfree(testptr);
    }

    //Large reallocation test
    testptr = rpmalloc(253000);
    testptr = rprealloc(testptr, 151);
    wanted_usable_size = (small_granularity * ((151 + (small_granularity - 1)) / small_granularity));
    if (rpmalloc_usable_size(testptr) != wanted_usable_size)
        return test_fail("Bad usable size");
    if (rpmalloc_usable_size(pointer_offset(testptr, 16)) != (wanted_usable_size - 16))
        return test_fail("Bad offset usable size");
    rpfree(testptr);

    //Reallocation tests
    for (iloop = 1; iloop < 24; ++iloop) {
        size_t size = 37 * iloop;
        testptr = rpmalloc(size);
        *((uintptr_t*)testptr) = 0x12345678;
        wanted_usable_size = small_granularity * ((size / small_granularity) + ((size % small_granularity) ? 1 : 0));
        if (rpmalloc_usable_size(testptr) != wanted_usable_size)
            return test_fail("Bad usable size (alloc)");
        testptr = rprealloc(testptr, size + 16);
        if (rpmalloc_usable_size(testptr) < (wanted_usable_size + 16))
            return test_fail("Bad usable size (realloc)");
        if (*((uintptr_t*)testptr) != 0x12345678)
            return test_fail("Data not preserved on realloc");
        rpfree(testptr);

        testptr = rpaligned_alloc(128, size);
        *((uintptr_t*)testptr) = 0x12345678;
        wanted_usable_size = small_granularity * ((size / small_granularity) + ((size % small_granularity) ? 1 : 0));
        if (rpmalloc_usable_size(testptr) < wanted_usable_size)
            return test_fail("Bad usable size (aligned alloc)");
        if (rpmalloc_usable_size(testptr) > (wanted_usable_size + 128))
            return test_fail("Bad usable size (aligned alloc)");
        testptr = rpaligned_realloc(testptr, 128, size + 32, 0, 0);
        if (rpmalloc_usable_size(testptr) < (wanted_usable_size + 32))
            return test_fail("Bad usable size (aligned realloc)");
        if (*((uintptr_t*)testptr) != 0x12345678)
            return test_fail("Data not preserved on realloc");
        if (rpaligned_realloc(testptr, 128, size * 1024 * 4, 0, RPMALLOC_GROW_OR_FAIL))
            return test_fail("Realloc with grow-or-fail did not fail as expected");
        void* unaligned = rprealloc(testptr, size);
        if (unaligned != testptr) {
            ptrdiff_t diff = pointer_diff(testptr, unaligned);
            if (diff < 0)
                return test_fail("Bad realloc behaviour");
            if (diff >= 128)
                return test_fail("Bad realloc behaviour");
        }
        rpfree(testptr);
    }

    static size_t alignment[5] = { 0, 32, 64, 128, 256 };
    for (iloop = 0; iloop < 5; ++iloop) {
        for (ipass = 0; ipass < 128 * 1024; ++ipass) {
            size_t this_alignment = alignment[iloop];
            char* baseptr = rpaligned_alloc(this_alignment, ipass);
            if (this_alignment && ((uintptr_t)baseptr & (this_alignment - 1)))
                return test_fail("Alignment failed");
            rpfree(baseptr);
        }
    }
    for (iloop = 0; iloop < 64; ++iloop) {
        for (ipass = 0; ipass < 8142; ++ipass) {
            size_t this_alignment = alignment[ipass % 5];
            size_t size = iloop + ipass + datasize[(iloop + ipass) % 7];
            char* baseptr = rpaligned_alloc(this_alignment, size);
            if (this_alignment && ((uintptr_t)baseptr & (this_alignment - 1)))
                return test_fail("Alignment failed");
            for (size_t ibyte = 0; ibyte < size; ++ibyte)
                baseptr[ibyte] = (char)(ibyte & 0xFF);

            size_t resize = (iloop * ipass + datasize[(iloop + ipass) % 7]) & 0x2FF;
            size_t capsize = (size > resize ? resize : size);
            baseptr = rprealloc(baseptr, resize);
            for (size_t ibyte = 0; ibyte < capsize; ++ibyte) {
                if (baseptr[ibyte] != (char)(ibyte & 0xFF))
                    return test_fail("Data not preserved on realloc");
            }

            size_t alignsize = (iloop * ipass + datasize[(iloop + ipass * 3) % 7]) & 0x2FF;
            this_alignment = alignment[(ipass + 1) % 5];
            capsize = (capsize > alignsize ? alignsize : capsize);
            baseptr = rpaligned_realloc(baseptr, this_alignment, alignsize, resize, 0);
            for (size_t ibyte = 0; ibyte < capsize; ++ibyte) {
                if (baseptr[ibyte] != (char)(ibyte & 0xFF))
                    return test_fail("Data not preserved on realloc");
            }

            rpfree(baseptr);
        }
    }

    for (iloop = 0; iloop < 64; ++iloop) {
        for (ipass = 0; ipass < 8142; ++ipass) {
            addr[ipass] = rpmalloc(500);
            if (addr[ipass] == 0)
                return test_fail("Allocation failed");

            memcpy(addr[ipass], data + ipass, 500);

            for (icheck = 0; icheck < ipass; ++icheck) {
                if (addr[icheck] == addr[ipass])
                    return test_fail("Bad allocation result");
                if (addr[icheck] < addr[ipass]) {
                    if (pointer_offset(addr[icheck], 500) > addr[ipass])
                        return test_fail("Bad allocation result");
                }
                else if (addr[icheck] > addr[ipass]) {
                    if (pointer_offset(addr[ipass], 500) > addr[icheck])
                        return test_fail("Bad allocation result");
                }
            }
        }

        for (ipass = 0; ipass < 8142; ++ipass) {
            if (memcmp(addr[ipass], data + ipass, 500))
                return test_fail("Data corruption");
        }

        for (ipass = 0; ipass < 8142; ++ipass)
            rpfree(addr[ipass]);
    }

    for (iloop = 0; iloop < 64; ++iloop) {
        for (ipass = 0; ipass < 1024; ++ipass) {
            unsigned int cursize = datasize[ipass%7] + ipass;

            addr[ipass] = rpmalloc(cursize);
            if (addr[ipass] == 0)
                return test_fail("Allocation failed");

            memcpy(addr[ipass], data + ipass, cursize);

            for (icheck = 0; icheck < ipass; ++icheck) {
                if (addr[icheck] == addr[ipass])
                    return test_fail("Identical pointer returned from allocation");
                if (addr[icheck] < addr[ipass]) {
                    if (pointer_offset(addr[icheck], rpmalloc_usable_size(addr[icheck])) > addr[ipass])
                        return test_fail("Invalid pointer inside another block returned from allocation");
                }
                else if (addr[icheck] > addr[ipass]) {
                    if (pointer_offset(addr[ipass], rpmalloc_usable_size(addr[ipass])) > addr[icheck])
                        return test_fail("Invalid pointer inside another block returned from allocation");
                }
            }
        }

        for (ipass = 0; ipass < 1024; ++ipass) {
            unsigned int cursize = datasize[ipass%7] + ipass;
            if (memcmp(addr[ipass], data + ipass, cursize))
                return test_fail("Data corruption");
        }

        for (ipass = 0; ipass < 1024; ++ipass)
            rpfree(addr[ipass]);
    }

    for (iloop = 0; iloop < 128; ++iloop) {
        for (ipass = 0; ipass < 1024; ++ipass) {
            addr[ipass] = rpmalloc(500);
            if (addr[ipass] == 0)
                return test_fail("Allocation failed");

            memcpy(addr[ipass], data + ipass, 500);

            for (icheck = 0; icheck < ipass; ++icheck) {
                if (addr[icheck] == addr[ipass])
                    return test_fail("Identical pointer returned from allocation");
                if (addr[icheck] < addr[ipass]) {
                    if (pointer_offset(addr[icheck], 500) > addr[ipass])
                        return test_fail("Invalid pointer inside another block returned from allocation");
                }
                else if (addr[icheck] > addr[ipass]) {
                    if (pointer_offset(addr[ipass], 500) > addr[icheck])
                        return test_fail("Invalid pointer inside another block returned from allocation");
                }
            }
        }

        for (ipass = 0; ipass < 1024; ++ipass) {
            if (memcmp(addr[ipass], data + ipass, 500))
                return test_fail("Data corruption");
        }

        for (ipass = 0; ipass < 1024; ++ipass)
            rpfree(addr[ipass]);
    }

    rpmalloc_finalize();

    for (iloop = 0; iloop < 2048; iloop += 16) {
        rpmalloc_initialize();
        addr[0] = rpmalloc(iloop);
        if (!addr[0])
            return test_fail("Allocation failed");
        rpfree(addr[0]);
        rpmalloc_finalize();
    }

    for (iloop = 2048; iloop < (64 * 1024); iloop += 512) {
        rpmalloc_initialize();
        addr[0] = rpmalloc(iloop);
        if (!addr[0])
            return test_fail("Allocation failed");
        rpfree(addr[0]);
        rpmalloc_finalize();
    }

    for (iloop = (64 * 1024); iloop < (2 * 1024 * 1024); iloop += 4096) {
        rpmalloc_initialize();
        addr[0] = rpmalloc(iloop);
        if (!addr[0])
            return test_fail("Allocation failed");
        rpfree(addr[0]);
        rpmalloc_finalize();
    }

    rpmalloc_initialize();
    for (iloop = 0; iloop < (2 * 1024 * 1024); iloop += 16) {
        addr[0] = rpmalloc(iloop);
        if (!addr[0])
            return test_fail("Allocation failed");
        rpfree(addr[0]);
    }
    rpmalloc_finalize();

    printf("Memory allocation tests passed\n");

    return 0;
}

static int
test_realloc(void) {
    srand((unsigned int)time(0));

    rpmalloc_initialize();

    size_t pointer_count = 4096;
    void** pointers = rpmalloc(sizeof(void*) * pointer_count);
    memset(pointers, 0, sizeof(void*) * pointer_count);

    size_t alignments[5] = {0, 16, 32, 64, 128};

    for (size_t iloop = 0; iloop < 8000; ++iloop) {
        for (size_t iptr = 0; iptr < pointer_count; ++iptr) {
            if (iloop)
                rpfree(rprealloc(pointers[iptr], (size_t)rand() % 4096));
            pointers[iptr] = rpaligned_alloc(alignments[(iptr + iloop) % 5], iloop + iptr);
        }
    }

    for (size_t iptr = 0; iptr < pointer_count; ++iptr)
        rpfree(pointers[iptr]);
    rpfree(pointers);

    size_t bigsize = 1024 * 1024;
    void* bigptr = rpmalloc(bigsize);
    while (bigsize < 3000000) {
        ++bigsize;
        bigptr = rprealloc(bigptr, bigsize);
        if (rpaligned_realloc(bigptr, 0, bigsize * 32, 0, RPMALLOC_GROW_OR_FAIL))
            return test_fail("Reallocation with grow-or-fail did not fail as expected");
        if (rpaligned_realloc(bigptr, 128, bigsize * 32, 0, RPMALLOC_GROW_OR_FAIL))
            return test_fail("Reallocation with aligned grow-or-fail did not fail as expected");
    }
    rpfree(bigptr);

    rpmalloc_finalize();

    printf("Memory reallocation tests passed\n");

    return 0;
}

static int
test_superalign(void) {

    rpmalloc_initialize();

    size_t alignment[] = { 2048, 4096, 8192, 16384, 32768 };
    size_t sizes[] = { 187, 1057, 2436, 5234, 9235, 17984, 35783, 72436 };

    for (size_t ipass = 0; ipass < 8; ++ipass) {
        for (size_t iloop = 0; iloop < 4096; ++iloop) {
            for (size_t ialign = 0, asize = sizeof(alignment) / sizeof(alignment[0]); ialign < asize; ++ialign) {
                for (size_t isize = 0, ssize = sizeof(sizes) / sizeof(sizes[0]); isize < ssize; ++isize) {
                    size_t alloc_size = sizes[isize] + iloop + ipass;
                    uint8_t* ptr = rpaligned_alloc(alignment[ialign], alloc_size);
                    if (!ptr || ((uintptr_t)ptr & (alignment[ialign] - 1)))
                        return test_fail("Super alignment allocation failed");
                    ptr[0] = 1;
                    ptr[alloc_size - 1] = 1;
                    rpfree(ptr);
                }
            }
        }
    }

    rpmalloc_finalize();

    printf("Memory super aligned tests passed\n");

    return 0;
}

typedef struct _allocator_thread_arg {
    unsigned int        loops;
    unsigned int        passes; //max 4096
    unsigned int        datasize[32];
    unsigned int        num_datasize; //max 32
    int                 init_fini_each_loop;
    void**              pointers;
    void**              crossthread_pointers;
} allocator_thread_arg_t;

static int got_error;

static void test_error_callback(const char* message) {
    printf("%s\n", message);
    (void)sizeof(message);
    got_error = 1;
}

static int test_error(void) {
    //printf("Detecting memory leak\n");

    rpmalloc_config_t config = {0};
    config.error_callback = test_error_callback;
    rpmalloc_initialize_config(&config);

    rpmalloc(10);

    rpmalloc_finalize();

    if (!got_error) {
        printf("Leak not detected and reported as expected\n");
        return -1;
    }

    printf("Error detection test passed\n");
    return 0;
}

int test_run(int argc, char** argv) {
    (void)sizeof(argc);
    (void)sizeof(argv);
    if (test_alloc())
        return -1;
    if (test_realloc())
        return -1;
    if (test_superalign())
        return -1;
    if (test_error())
        return -1;
    printf("All tests passed\n");
    return 0;
}

int main(int argc, char** argv) {
    return test_run(argc, argv);
}
