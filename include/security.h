/*
 * torOS Security Subsystem Header
 * Privilege levels, sandboxing, ASLR, NX bit
 */

#ifndef _SECURITY_H
#define _SECURITY_H

#include "toros.h"

/* ===== Privilege Levels ===== */
#define PRIV_KERNEL         0
#define PRIV_DRIVER         1
#define PRIV_SERVICE        2
#define PRIV_USER           3
#define PRIV_GUEST          4
#define NUM_PRIV_LEVELS     5

#define CAP_IO              (1 << 0)
#define CAP_NET             (1 << 1)
#define CAP_FS              (1 << 2)
#define CAP_PROC            (1 << 3)
#define CAP_SYS             (1 << 4)
#define CAP_KILL            (1 << 5)
#define CAP_DEBUG           (1 << 6)

#define NUM_CAPS            7

/* Capability set */
typedef uint32 cap_set_t;

/* Security context per process */
typedef struct {
    uint32 uid;
    uint32 gid;
    uint32 euid;
    uint32 egid;
    uint32 priv_level;
    cap_set_t caps;
    cap_set_t cap_inheritable;
    cap_set_t cap_permitted;
    cap_set_t cap_effective;
    uint32 in_sandbox;
} security_context_t;

/* ===== Sandboxing ===== */
#define SANDBOX_MAX_RULES   32
#define RULE_ALLOW          1
#define RULE_DENY           0

typedef enum {
    RULE_FS_READ,
    RULE_FS_WRITE,
    RULE_FS_EXEC,
    RULE_NET_CONNECT,
    RULE_NET_LISTEN,
    RULE_PROC_FORK,
    RULE_PROC_EXEC,
    RULE_SYS_REBOOT,
    RULE_SYS_TIME
} sandbox_rule_type_t;

typedef struct {
    sandbox_rule_type_t type;
    int action;         /* RULE_ALLOW or RULE_DENY */
    char pattern[128];  /* Path or pattern */
} sandbox_rule_t;

typedef struct {
    sandbox_rule_t rules[SANDBOX_MAX_RULES];
    int num_rules;
    int active;
    char name[32];
} sandbox_t;

/* ===== ASLR ===== */
#define ASLR_ENABLED        1
#define ASLR_BITS_STACK     16
#define ASLR_BITS_MMAP      16
#define ASLR_BITS_PIE       16

/* ===== NX Bit ===== */
#define PAGE_NX             (1 << 63)

/* ===== Security API ===== */
void security_init(void);
void security_context_init(security_context_t *ctx, uint32 priv_level);
int security_check_cap(security_context_t *ctx, uint32 cap);
int security_grant_cap(security_context_t *ctx, uint32 cap);
int security_revoke_cap(security_context_t *ctx, uint32 cap);
int security_check_access(security_context_t *ctx, const char *path, uint32 access);

/* Privilege levels */
void security_set_privilege(security_context_t *ctx, uint32 level);
uint32 security_get_privilege(security_context_t *ctx);
const char *security_priv_name(uint32 level);

/* Sandboxing */
void sandbox_init(sandbox_t *box, const char *name);
int sandbox_add_rule(sandbox_t *box, sandbox_rule_type_t type, int action, const char *pattern);
int sandbox_check(sandbox_t *box, sandbox_rule_type_t type, const char *target);
void sandbox_enter(sandbox_t *box);
void sandbox_leave(sandbox_t *box);
void sandbox_current(sandbox_t *box);

/* ASLR */
void aslr_init(void);
uint64 aslr_randomize_stack(uint64 base);
uint64 aslr_randomize_mmap(uint64 base);
uint64 aslr_randomize_pie(uint64 base);
int aslr_enabled(void);
void aslr_set_enabled(int enable);

/* NX Bit */
void nx_init(void);
int nx_page_set(void *page_table, uint64 va, int nx);
int nx_page_clear(void *page_table, uint64 va);
int nx_is_supported(void);

/* MAC (Mandatory Access Control) */
#define MAC_LEVEL_UNCLASSIFIED  0
#define MAC_LEVEL_CONFIDENTIAL  1
#define MAC_LEVEL_SECRET        2
#define MAC_LEVEL_TOPSECRET     3

typedef struct {
    uint32 mac_level;
    uint32 mac_categories;  /* Category bitmap */
} mac_label_t;

void mac_init(void);
int mac_check_access(mac_label_t *subject, mac_label_t *object, uint32 access);
void mac_set_process_label(uint32 level, uint32 categories);
void mac_set_file_label(const char *path, uint32 level, uint32 categories);

/* Audit */
void audit_init(void);
void audit_log(const char *event, security_context_t *ctx, const char *detail, int result);
void audit_dump(void);

#endif
