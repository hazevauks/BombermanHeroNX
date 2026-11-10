// SD_LOG/bmhero_crash.txt: what the CPU was doing when the process faulted.
//
// Addresses are also given relative to the start of the program, to feed to addr2line with the ELF of the build:
//
//     aarch64-none-elf-addr2line -f -C -e build/switch/BombermanHeroNX.elf <offset>
#include "nx_platform.h"

#include <cstdio>

#include <switch.h>

namespace nx {
    void log_flush_unlocked();
}

extern "C" {
    alignas(16) u8 __nx_exception_stack[0x8000];
    u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

    extern char __start__[]; // the program's load address, from libnx's crt0
}

namespace {
    constexpr int MAX_BACKTRACE = 48;

    const char *exception_name(u32 description) {
        switch (description) {
        case ThreadExceptionDesc_InstructionAbort: return "instruction abort";
        case ThreadExceptionDesc_MisalignedPC: return "misaligned PC";
        case ThreadExceptionDesc_MisalignedSP: return "misaligned SP";
        case ThreadExceptionDesc_SError: return "SError";
        case ThreadExceptionDesc_BadSVC: return "bad SVC";
        case ThreadExceptionDesc_Trap: return "trap";
        case ThreadExceptionDesc_Other: return "data abort or other";
        default: return "unknown";
        }
    }

    bool is_readable(u64 address, u64 size) {
        MemoryInfo info;
        u32 page;
        if ((address & 7) != 0 || R_FAILED(svcQueryMemory(&info, &page, address))) {
            return false;
        }
        return (info.perm & Perm_R) != 0 && address + size <= info.addr + info.size;
    }

    void report(FILE *file, const ThreadExceptionDump *context) {
        const u64 base = u64(__start__);
        u64 frame_pointer = context->fp.x;

        fprintf(file, "=== BombermanHeroNX crash report ===\n");
        fprintf(file, "exception: %s (0x%x)\n", exception_name(context->error_desc), context->error_desc);
        fprintf(file, "base: 0x%016lx\n", base);
        fprintf(file, "pc  : 0x%016lx (offset 0x%lx)\n", context->pc.x, context->pc.x - base);
        fprintf(file, "lr  : 0x%016lx (offset 0x%lx)\n", context->lr.x, context->lr.x - base);
        fprintf(file, "sp  : 0x%016lx\n", context->sp.x);
        fprintf(file, "fp  : 0x%016lx\n", context->fp.x);
        fprintf(file, "far : 0x%016lx\n", context->far.x);
        fprintf(file, "esr : 0x%08x  pstate: 0x%08x\n", context->esr, context->pstate);

        for (int i = 0; i < 29; i++) {
            fprintf(file, "x%-2d : 0x%016lx%s", i, context->cpu_gprs[i].x, (i & 1) ? "\n" : "  ");
        }
        fprintf(file, "\n");

        fprintf(file, "backtrace (offsets):\n");
        fprintf(file, "  0x%lx\n  0x%lx\n", context->pc.x - base, context->lr.x - base);
        for (int i = 0; i < MAX_BACKTRACE && is_readable(frame_pointer, 16); i++) {
            const u64 *frame = reinterpret_cast<const u64 *>(frame_pointer);
            if (frame[1] == 0) {
                break;
            }
            fprintf(file, "  0x%lx\n", frame[1] - base);
            if (frame[0] <= frame_pointer) {
                break;
            }
            frame_pointer = frame[0];
        }
    }
}

extern "C" void __libnx_exception_handler(ThreadExceptionDump *context) {
    // Straight to the files: the thread that faulted may hold the log's lock.
    fprintf(stderr, "\nFATAL: %s at offset 0x%lx, see " NX_CRASH_FILE "\n",
        exception_name(context->error_desc), context->pc.x - u64(__start__));
    nx::log_flush_unlocked();

    FILE *file = fopen(NX_CRASH_FILE, "w");
    if (file != nullptr) {
        report(file, context);
        fclose(file);
    }
}
