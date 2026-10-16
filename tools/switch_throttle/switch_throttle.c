// Runs a program with roughly the CPU power a Switch game gets: 3 cores, each about RATIO times as fast as one
// core of this PC. Every thread of the child is suspended and resumed in a short fixed period (run RATIO of it,
// stop the rest), so every thread slows down by the same factor, like on a slower CPU. The GPU is not limited.
// Usage: switch_throttle.exe [--ratio 0.15] [--cores 3] [--period-us 4000] [--pid N | -- command args...]
// Build: tools/switch_throttle/build.sh. See docs/switch.md "Switch-speed test on PC".
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_THREADS 1024

static DWORD g_tids[MAX_THREADS];
static HANDLE g_handles[MAX_THREADS];
static int g_count;

// Adds threads of pid that are not in the list yet (threads that exited stay listed; suspending them fails
// harmlessly).
static void refresh_threads(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te = {sizeof(te)};
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        int known = 0;
        for (int i = 0; i < g_count && !known; i++) known = g_tids[i] == te.th32ThreadID;
        if (known || g_count == MAX_THREADS) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!h) continue;
        g_tids[g_count] = te.th32ThreadID;
        g_handles[g_count++] = h;
    }
    CloseHandle(snap);
}

static void wait_us(HANDLE timer, LONGLONG us) {
    LARGE_INTEGER due;
    due.QuadPart = -us * 10;  // relative, 100 ns units
    SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
}

// Command line of the child: everything after "--", quoted as given to us.
static char *child_command_line(void) {
    char *cl = GetCommandLineA();
    char *sep = strstr(cl, " -- ");
    return sep ? sep + 4 : NULL;
}

int main(int argc, char **argv) {
    double ratio = 0.15;
    int cores = 3;
    LONGLONG period_us = 4000;
    DWORD pid = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--")) break;
        if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", argv[i]); return 2; }
        if (!strcmp(argv[i], "--ratio")) ratio = atof(argv[++i]);
        else if (!strcmp(argv[i], "--cores")) cores = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--period-us")) period_us = _atoi64(argv[++i]);
        else if (!strcmp(argv[i], "--pid")) pid = (DWORD)atoi(argv[++i]);
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }
    if (ratio <= 0 || ratio > 1 || cores < 1 || period_us < 500) { fprintf(stderr, "bad options\n"); return 2; }

    HANDLE proc;
    if (pid) {
        proc = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
        if (!proc) { fprintf(stderr, "cannot open pid %lu (%lu)\n", pid, GetLastError()); return 1; }
    } else {
        char *cmd = child_command_line();
        if (!cmd) { fprintf(stderr, "usage: switch_throttle [options] (--pid N | -- command args...)\n"); return 2; }
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            fprintf(stderr, "cannot start: %s (%lu)\n", cmd, GetLastError());
            return 1;
        }
        CloseHandle(pi.hThread);
        proc = pi.hProcess;
        pid = pi.dwProcessId;
    }

    // One logical processor per physical core (even indices: the SMT siblings stay free), like the Switch's
    // separate cores.
    DWORD_PTR mask = 0;
    for (int c = 0; c < cores; c++) mask |= (DWORD_PTR)1 << (2 * c);
    if (!SetProcessAffinityMask(proc, mask)) fprintf(stderr, "affinity failed (%lu)\n", GetLastError());

    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    timeBeginPeriod(1);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    LONGLONG run_us = (LONGLONG)(period_us * ratio);
    fprintf(stderr, "switch_throttle: pid %lu, %d cores, run %lld of every %lld us\n", pid, cores, run_us, period_us);
    LARGE_INTEGER freq, last_scan, now;
    QueryPerformanceFrequency(&freq);
    last_scan.QuadPart = 0;
    while (WaitForSingleObject(proc, 0) == WAIT_TIMEOUT) {
        QueryPerformanceCounter(&now);
        if ((now.QuadPart - last_scan.QuadPart) * 10 > freq.QuadPart) {  // new threads: rescan every 100 ms
            refresh_threads(pid);
            last_scan = now;
        }
        wait_us(timer, run_us);
        for (int i = 0; i < g_count; i++) SuspendThread(g_handles[i]);
        wait_us(timer, period_us - run_us);
        for (int i = 0; i < g_count; i++) ResumeThread(g_handles[i]);
    }
    DWORD code = 0;
    GetExitCodeProcess(proc, &code);
    timeEndPeriod(1);
    return (int)code;
}
