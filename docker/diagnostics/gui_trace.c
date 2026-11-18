/* Diagnóstico opt-in de crash; não altera o fluxo normal da GUI. GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

static void frame(void *address)
{
    Dl_info info;
    if (dladdr(address, &info) && info.dli_fname && info.dli_fbase)
        dprintf(STDERR_FILENO, "CARMEN_FRAME path=%s offset=0x%" PRIxPTR
                " address=0x%" PRIxPTR "\n", info.dli_fname,
                (uintptr_t)address - (uintptr_t)info.dli_fbase, (uintptr_t)address);
}

static void crash(int sig, siginfo_t *info, void *context)
{
    void *frames[64];
    dprintf(STDERR_FILENO, "\nCARMEN_GUI_CRASH signal=%d address=%p\n", sig, info->si_addr);
#if defined(__x86_64__)
    /* A primeira entrada é a instrução que falhou, independente do unwind. */
    ucontext_t *uc = (ucontext_t *)context;
    frame((void *)(uintptr_t)uc->uc_mcontext.gregs[REG_RIP]);
#else
    (void)context;
#endif
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
    for (int i = 0; i < count; ++i) frame(frames[i]);
    /* SA_RESETHAND já restaurou a ação padrão. Preserva a morte pelo sinal. */
    kill(getpid(), sig);
}

__attribute__((constructor)) static void install_trace(void)
{
    void *warmup[1];
    /* Carrega o unwinder antes da falha. O diagnóstico é best-effort, com timeout. */
    backtrace(warmup, 1);
    setvbuf(stdout, NULL, _IONBF, 0);
    struct sigaction action = {0};
    action.sa_sigaction = crash;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, NULL) || sigaction(SIGBUS, &action, NULL) ||
        sigaction(SIGABRT, &action, NULL)) {
        perror("sigaction: diagnóstico GUI");
        _exit(2);
    }
    dprintf(STDERR_FILENO, "CARMEN_GUI_TRACE: captura de falha habilitada\n");
}
