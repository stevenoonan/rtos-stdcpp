/* GCC emulated TLS is supplied by rtos-stdcpp. Reject native ELF TLS, which
 * would require a different runtime and port integration.
 * The generated RAM region also link-checks both fixed BSS heap arenas.
 */
ASSERT(SIZEOF(.tdata) == 0 && SIZEOF(.tbss) == 0,
       "Native ELF TLS is unsupported: use the qualified GCC emulated TLS runtime")
ASSERT(_ebss <= _sstack, "Runtime data/heaps overlap the MSP reservation")
