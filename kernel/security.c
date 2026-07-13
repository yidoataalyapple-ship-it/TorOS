/*
 * torOS Security Subsystem
 * Privilege levels, sandboxing, ASLR, NX bit, MAC, audit
 */

#include "../include/toros.h"
#include "../include/security.h"

static security_context_t kernel_ctx;
static sandbox_t *active_sandbox = NULL;
static int nx_supported = 0;
static int aslr_on = ASLR_ENABLED;
static mac_label_t process_mac_label;

/* Privilege names */
static const char *priv_names[NUM_PRIV_LEVELS] = {
    "Kernel", "Driver", "Service", "User", "Guest"
};

/* Capability names */
static const char *cap_names[NUM_CAPS] = {
    "IO", "Network", "Filesystem", "Process", "System", "Kill", "Debug"
};

/* Default capabilities per level */
static const cap_set_t default_caps[NUM_PRIV_LEVELS] = {
    [PRIV_KERNEL]  = CAP_IO | CAP_NET | CAP_FS | CAP_PROC | CAP_SYS | CAP_KILL | CAP_DEBUG,
    [PRIV_DRIVER]  = CAP_IO | CAP_NET | CAP_FS | CAP_PROC,
    [PRIV_SERVICE] = CAP_NET | CAP_FS | CAP_PROC,
    [PRIV_USER]    = CAP_FS | CAP_PROC,
    [PRIV_GUEST]   = 0,
};

/* ===== Core Security ===== */

void security_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Security Subsystem...\n");

    /* Initialize kernel context */
    memset(&kernel_ctx, 0, sizeof(security_context_t));
    kernel_ctx.uid = 0;
    kernel_ctx.gid = 0;
    kernel_ctx.priv_level = PRIV_KERNEL;
    kernel_ctx.caps = default_caps[PRIV_KERNEL];
    kernel_ctx.cap_permitted = kernel_ctx.caps;
    kernel_ctx.cap_effective = kernel_ctx.caps;

    /* Check NX support (AArch64 always has PXN/XN) */
    nx_supported = 1;

    /* Init MAC */
    memset(&process_mac_label, 0, sizeof(mac_label_t));
    process_mac_label.mac_level = MAC_LEVEL_TOPSECRET;
    process_mac_label.mac_categories = 0xFFFFFFFF;

    /* Init ASLR entropy source */
    aslr_init();

    printk_color(TERM_GREEN, "[BOOT] Security: PrivLevels=%d, NX=%s, ASLR=%s\n",
                 NUM_PRIV_LEVELS,
                 nx_supported ? "yes" : "no",
                 aslr_on ? "on" : "off");
}

void security_context_init(security_context_t *ctx, uint32 priv_level)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(security_context_t));
    ctx->uid = (priv_level == PRIV_KERNEL) ? 0 : 1000;
    ctx->gid = (priv_level == PRIV_KERNEL) ? 0 : 1000;
    ctx->euid = ctx->uid;
    ctx->egid = ctx->gid;
    ctx->priv_level = priv_level;
    ctx->caps = default_caps[priv_level];
    ctx->cap_permitted = ctx->caps;
    ctx->cap_effective = ctx->caps;
    ctx->in_sandbox = 0;
}

int security_check_cap(security_context_t *ctx, uint32 cap)
{
    if (!ctx) return -1;
    if (ctx->priv_level == PRIV_KERNEL) return 0; /* Kernel always allowed */
    return (ctx->cap_effective & cap) ? 0 : -1;
}

int security_grant_cap(security_context_t *ctx, uint32 cap)
{
    if (!ctx || ctx->priv_level != PRIV_KERNEL) return -1;
    ctx->cap_permitted |= cap;
    ctx->cap_effective |= cap;
    return 0;
}

int security_revoke_cap(security_context_t *ctx, uint32 cap)
{
    if (!ctx) return -1;
    ctx->cap_effective &= ~cap;
    return 0;
}

void security_set_privilege(security_context_t *ctx, uint32 level)
{
    if (!ctx || level >= NUM_PRIV_LEVELS) return;
    ctx->priv_level = level;
    ctx->caps = default_caps[level];
    ctx->cap_permitted = ctx->caps;
    ctx->cap_effective = ctx->caps;
}

uint32 security_get_privilege(security_context_t *ctx)
{
    return ctx ? ctx->priv_level : PRIV_GUEST;
}

const char *security_priv_name(uint32 level)
{
    if (level < NUM_PRIV_LEVELS) return priv_names[level];
    return "Unknown";
}

int security_check_access(security_context_t *ctx, const char *path, uint32 access)
{
    if (!ctx || !path) return -1;

    /* Sandbox check first */
    if (ctx->in_sandbox && active_sandbox && active_sandbox->active) {
        sandbox_rule_type_t rule_type;
        if (access & 1) rule_type = RULE_FS_READ;
        else if (access & 2) rule_type = RULE_FS_WRITE;
        else if (access & 4) rule_type = RULE_FS_EXEC;
        else return 0;

        if (sandbox_check(active_sandbox, rule_type, path) != RULE_ALLOW)
            return -1;
    }

    /* Capability check */
    if ((access & 1) && security_check_cap(ctx, CAP_FS) < 0) return -1;
    if ((access & 2) && security_check_cap(ctx, CAP_FS) < 0) return -1;

    return 0;
}

/* ===== Sandboxing ===== */

void sandbox_init(sandbox_t *box, const char *name)
{
    if (!box) return;
    memset(box, 0, sizeof(sandbox_t));
    strncpy(box->name, name ? name : "sandbox", 31);
    box->active = 0;
    box->num_rules = 0;

    /* Default: deny everything */
    for (int i = 0; i < SANDBOX_MAX_RULES; i++) {
        box->rules[i].action = RULE_DENY;
        box->rules[i].type = RULE_FS_READ;
        box->rules[i].pattern[0] = '*';
    }
}

int sandbox_add_rule(sandbox_t *box, sandbox_rule_type_t type, int action, const char *pattern)
{
    if (!box || box->num_rules >= SANDBOX_MAX_RULES) return -1;
    sandbox_rule_t *rule = &box->rules[box->num_rules++];
    rule->type = type;
    rule->action = action;
    strncpy(rule->pattern, pattern ? pattern : "*", 127);
    rule->pattern[127] = '\0';
    return 0;
}

int sandbox_check(sandbox_t *box, sandbox_rule_type_t type, const char *target)
{
    if (!box || !box->active) return RULE_ALLOW;

    /* Check rules (last match wins) */
    int result = RULE_DENY; /* Default deny */
    for (int i = 0; i < box->num_rules; i++) {
        if (box->rules[i].type == type) {
            /* Pattern match */
            if (box->rules[i].pattern[0] == '*' ||
                (target && strstr(target, box->rules[i].pattern))) {
                result = box->rules[i].action;
            }
        }
    }
    return result;
}

void sandbox_enter(sandbox_t *box)
{
    if (!box) return;
    box->active = 1;
    active_sandbox = box;
    kernel_ctx.in_sandbox = 1;
    printk_color(TERM_YELLOW, "[SECURITY] Entered sandbox: %s (%d rules)\n",
                 box->name, box->num_rules);
}

void sandbox_leave(sandbox_t *box)
{
    if (!box) return;
    box->active = 0;
    if (active_sandbox == box) active_sandbox = NULL;
    kernel_ctx.in_sandbox = 0;
    printk_color(TERM_GREEN, "[SECURITY] Left sandbox: %s\n", box->name);
}

void sandbox_current(sandbox_t *box)
{
    if (!box) return;
    if (active_sandbox) memcpy(box, active_sandbox, sizeof(sandbox_t));
    else memset(box, 0, sizeof(sandbox_t));
}

/* ===== ASLR ===== */

static uint64 aslr_entropy = 0;

void aslr_init(void)
{
    /* Use timer as entropy source */
    aslr_entropy = r_cntvct();
    aslr_on = ASLR_ENABLED;
    printk_color(TERM_GREEN, "[SECURITY] ASLR initialized\n");
}

static uint64 aslr_random_bits(int bits)
{
    aslr_entropy = aslr_entropy * 1103515245 + 12345;
    return aslr_entropy & ((1ULL << bits) - 1);
}

uint64 aslr_randomize_stack(uint64 base)
{
    if (!aslr_on) return base;
    uint64 offset = aslr_random_bits(ASLR_BITS_STACK);
    offset &= ~0xFFFULL; /* Page align */
    return base - offset;
}

uint64 aslr_randomize_mmap(uint64 base)
{
    if (!aslr_on) return base;
    uint64 offset = aslr_random_bits(ASLR_BITS_MMAP);
    offset &= ~0xFFFULL;
    return base + offset;
}

uint64 aslr_randomize_pie(uint64 base)
{
    if (!aslr_on) return base;
    uint64 offset = aslr_random_bits(ASLR_BITS_PIE);
    offset &= ~0xFFFULL;
    return base + offset;
}

int aslr_enabled(void) { return aslr_on; }
void aslr_set_enabled(int enable) { aslr_on = enable; }

/* ===== NX Bit ===== */

void nx_init(void)
{
    /* AArch64: Check if FEAT_XNX is supported */
    uint64 mmfr1;
    __asm__ volatile("mrs %0, ID_AA64MMFR1_EL1" : "=r"(mmfr1));
    /* XNX bits [35:32], non-zero = supported */
    nx_supported = ((mmfr1 >> 32) & 0xF) != 0;
    printk_color(TERM_GREEN, "[SECURITY] NX: %s\n", nx_supported ? "supported" : "basic");
}

int nx_is_supported(void) { return nx_supported; }

int nx_page_set(void *page_table, uint64 va, int nx)
{
    (void)page_table;
    if (!nx_supported) return -1;
    /* On AArch64, set the PXN/XN bits in page table entry */
    /* This is a simplified version - real implementation needs page table walk */
    uint64 *pte = (uint64 *)vm_va2pa(va);
    if (!pte) return -1;
    if (nx) *pte |= PAGE_NX;
    else *pte &= ~PAGE_NX;
    __asm__ volatile("dsb ishst; tlbi vmalle1is; dsb ish; isb" ::: "memory");
    return 0;
}

int nx_page_clear(void *page_table, uint64 va)
{
    return nx_page_set(page_table, va, 0);
}

/* ===== MAC (Mandatory Access Control) ===== */

void mac_init(void)
{
    printk_color(TERM_GREEN, "[SECURITY] MAC initialized\n");
}

int mac_check_access(mac_label_t *subject, mac_label_t *object, uint32 access)
{
    (void)access;
    if (!subject || !object) return -1;
    /* Simple Bell-LaPadula: no read up, no write down */
    if (subject->mac_level < object->mac_level) return -1; /* No read up */
    if (subject->mac_level > object->mac_level) return -1; /* No write down */
    /* Check category intersection */
    if ((subject->mac_categories & object->mac_categories) == 0) return -1;
    return 0;
}

void mac_set_process_label(uint32 level, uint32 categories)
{
    process_mac_label.mac_level = level;
    process_mac_label.mac_categories = categories;
}

void mac_set_file_label(const char *path, uint32 level, uint32 categories)
{
    (void)path; (void)level; (void)categories;
    /* Would store in filesystem extended attributes */
}

/* ===== Audit ===== */

#define AUDIT_MAX_ENTRIES   256

typedef struct {
    uint64 timestamp;
    char event[32];
    uint32 uid;
    uint32 priv_level;
    char detail[128];
    int result;
} audit_entry_t;

static audit_entry_t audit_log_entries[AUDIT_MAX_ENTRIES];
static int audit_count = 0;
static int audit_enabled = 1;
static spinlock_t audit_lock;

void audit_init(void)
{
    memset(audit_log_entries, 0, sizeof(audit_log_entries));
    audit_count = 0;
    audit_enabled = 1;
    spin_init(&audit_lock);
    printk_color(TERM_GREEN, "[SECURITY] Audit initialized\n");
}

void audit_log(const char *event, security_context_t *ctx, const char *detail, int result)
{
    if (!audit_enabled || !event) return;

    spin_lock(&audit_lock);
    int idx = audit_count % AUDIT_MAX_ENTRIES;
    audit_entry_t *entry = &audit_log_entries[idx];

    entry->timestamp = rtc_get_time();
    strncpy(entry->event, event, 31);
    entry->uid = ctx ? ctx->uid : 0;
    entry->priv_level = ctx ? ctx->priv_level : PRIV_KERNEL;
    strncpy(entry->detail, detail ? detail : "", 127);
    entry->result = result;

    audit_count++;
    spin_unlock(&audit_lock);
}

void audit_dump(void)
{
    printk_color(TERM_CYAN, "\n=== Audit Log (%d entries) ===\n", audit_count);
    int start = (audit_count > AUDIT_MAX_ENTRIES) ? audit_count - AUDIT_MAX_ENTRIES : 0;
    for (int i = start; i < audit_count; i++) {
        audit_entry_t *e = &audit_log_entries[i % AUDIT_MAX_ENTRIES];
        printk("  [%lu] %s uid=%d priv=%s: %s (result=%d)\n",
               e->timestamp, e->event, e->uid,
               security_priv_name(e->priv_level), e->detail, e->result);
    }
    printk("\n");
}
