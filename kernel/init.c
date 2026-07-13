/*
 * torOS Boot & Init Subsystem
 * Multi-stage boot, init system, driver auto-loading, login manager
 */

#include "../include/toros.h"
#include "../include/init.h"
#include "../include/security.h"
#include "../include/window.h"
#include "../include/network.h"
#include "../include/audio.h"
#include "../include/gpu.h"
#include "../include/font.h"

static service_t services[INIT_MAX_SERVICES];
static int num_services = 0;
static driver_t drivers[DRIVER_MAX];
static int num_drivers = 0;
static user_account_t users[LOGIN_MAX_USERS];
static int num_users = 0;
static user_session_t sessions[LOGIN_MAX_SESSIONS];
static int login_active = 0;

/* ===== Service Management ===== */

void init_system_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Init System...\n");

    memset(services, 0, sizeof(services));
    num_services = 0;

    /* Register default services */
    init_register_service("drivers", "", "Hardware driver loader", 1, NULL);
    init_register_service("filesystem", "", "VFS and filesystems", 1, NULL);
    init_register_service("network", "", "Network stack", 1, NULL);
    init_register_service("audio", "", "Audio subsystem", 1, NULL);
    init_register_service("gpu", "", "GPU and display", 1, NULL);
    init_register_service("input", "", "Input devices", 1, NULL);
    init_register_service("windowing", "", "Window manager", 1, NULL);
    init_register_service("security", "", "Security subsystem", 1, NULL);

    printk_color(TERM_GREEN, "[BOOT] Init system: %d services registered\n", num_services);
}

int init_register_service(const char *name, const char *command,
                          const char *description, uint32 auto_start,
                          int (*init_func)(void))
{
    if (num_services >= INIT_MAX_SERVICES || !name) return -1;

    service_t *svc = &services[num_services++];
    strncpy(svc->name, name, INIT_SERVICE_NAME_LEN - 1);
    strncpy(svc->command, command ? command : "", INIT_CMD_LEN - 1);
    strncpy(svc->description, description ? description : "", 63);
    svc->state = SERVICE_STOPPED;
    svc->auto_start = auto_start;
    svc->restart_on_crash = 0;
    svc->max_restarts = 5;
    svc->restart_count = 0;
    svc->init_func = init_func;
    svc->num_dependencies = 0;
    svc->initialized = 0;
    return 0;
}

int init_start_service(const char *name)
{
    for (int i = 0; i < num_services; i++) {
        if (strcmp(services[i].name, name) == 0) {
            service_t *svc = &services[i];
            if (svc->state == SERVICE_RUNNING) return 0;

            svc->state = SERVICE_STARTING;
            printk_color(TERM_CYAN, "[INIT] Starting: %s\n", name);

            /* Check dependencies */
            for (int d = 0; d < svc->num_dependencies; d++) {
                init_start_service(svc->dependencies[d]);
            }

            /* Run init function */
            if (svc->init_func) {
                if (svc->init_func() < 0) {
                    svc->state = SERVICE_FAILED;
                    printk_color(TERM_RED, "[INIT] Failed: %s\n", name);
                    return -1;
                }
            }

            svc->state = SERVICE_RUNNING;
            svc->start_time = get_jiffies();
            svc->initialized = 1;
            printk_color(TERM_GREEN, "[INIT] Running: %s\n", name);
            return 0;
        }
    }
    printk_color(TERM_RED, "[INIT] Unknown service: %s\n", name);
    return -1;
}

int init_stop_service(const char *name)
{
    for (int i = 0; i < num_services; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].state = SERVICE_STOPPED;
            printk_color(TERM_YELLOW, "[INIT] Stopped: %s\n", name);
            return 0;
        }
    }
    return -1;
}

void init_run_services(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Auto-starting services...\n");
    for (int i = 0; i < num_services; i++) {
        if (services[i].auto_start) {
            init_start_service(services[i].name);
        }
    }
}

void init_list_services(void)
{
    printk_color(TERM_CYAN, "\n=== Services ===\n");
    const char *state_names[] = {"Stopped", "Starting", "Running", "Stopping", "Failed", "Restarting"};
    for (int i = 0; i < num_services; i++) {
        service_t *s = &services[i];
        printk("  [%c] %-16s %s (%s)\n",
               s->state == SERVICE_RUNNING ? '*' : ' ',
               s->name, state_names[s->state], s->description);
    }
    printk("\n");
}

/* ===== Driver Loading ===== */

/* Built-in driver init functions (from other subsystems) */
extern void virtio_input_init(void);
extern void virtio_gpu_init(void);
extern void audio_init(void);
extern void net_init(void);
extern void block_init(void);
extern void font_system_init(void);

/* Wrappers: driver framework expects int-returning init functions */
static int wrap_virtio_input_init(void) { virtio_input_init(); return 0; }
static int wrap_virtio_gpu_init(void)   { virtio_gpu_init(); return 0; }
static int wrap_audio_init(void)        { audio_init(); return 0; }
static int wrap_net_init(void)          { net_init(); return 0; }
static int wrap_block_init(void)        { block_init(); return 0; }
static int wrap_font_init(void)         { font_system_init(); return 0; }

void driver_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Driver Manager...\n");
    memset(drivers, 0, sizeof(drivers));
    num_drivers = 0;

    /* Register built-in drivers (wrappers defined at file scope) */

    driver_register("virtio-input", DRIVER_TYPE_INPUT, NULL, wrap_virtio_input_init, NULL);
    driver_register("virtio-gpu", DRIVER_TYPE_GPU, NULL, wrap_virtio_gpu_init, NULL);
    driver_register("audio", DRIVER_TYPE_SOUND, NULL, wrap_audio_init, NULL);
    driver_register("network", DRIVER_TYPE_NET, NULL, wrap_net_init, NULL);
    driver_register("block", DRIVER_TYPE_BLOCK, NULL, wrap_block_init, NULL);
    driver_register("font", DRIVER_TYPE_PCI, NULL, wrap_font_init, NULL);

    printk_color(TERM_GREEN, "[BOOT] Driver manager: %d drivers registered\n", num_drivers);
}

int driver_register(const char *name, driver_type_t type,
                    int (*probe)(void), int (*init_func)(void), void (*shutdown_func)(void))
{
    if (num_drivers >= DRIVER_MAX || !name) return -1;

    driver_t *drv = &drivers[num_drivers++];
    strncpy(drv->name, name, DRIVER_NAME_LEN - 1);
    drv->type = type;
    drv->probe = probe;
    drv->init = init_func;
    drv->shutdown = shutdown_func;
    drv->loaded = 0;
    return 0;
}

int driver_load_all(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Loading drivers...\n");
    int loaded = 0;
    for (int i = 0; i < num_drivers; i++) {
        if (!drivers[i].loaded) {
            printk_color(TERM_CYAN, "[DRIVER] Loading: %s\n", drivers[i].name);

            int probe_ok = 1;
            if (drivers[i].probe) {
                probe_ok = (drivers[i].probe() == 0);
            }

            if (probe_ok && drivers[i].init) {
                if (drivers[i].init() == 0) {
                    drivers[i].loaded = 1;
                    loaded++;
                    printk_color(TERM_GREEN, "[DRIVER] %s: OK\n", drivers[i].name);
                } else {
                    printk_color(TERM_YELLOW, "[DRIVER] %s: Init failed\n", drivers[i].name);
                }
            } else if (!probe_ok) {
                printk_color(TERM_YELLOW, "[DRIVER] %s: Not detected\n", drivers[i].name);
            }
        }
    }
    printk_color(TERM_GREEN, "[BOOT] %d/%d drivers loaded\n", loaded, num_drivers);
    return loaded;
}

int driver_load_by_type(driver_type_t type)
{
    int loaded = 0;
    for (int i = 0; i < num_drivers; i++) {
        if (drivers[i].type == type && !drivers[i].loaded && drivers[i].init) {
            if (drivers[i].init() == 0) {
                drivers[i].loaded = 1;
                loaded++;
            }
        }
    }
    return loaded;
}

void driver_list(void)
{
    printk_color(TERM_CYAN, "\n=== Drivers ===\n");
    for (int i = 0; i < num_drivers; i++) {
        const char *type_str;
        switch (drivers[i].type) {
        case DRIVER_TYPE_PCI: type_str = "PCI"; break;
        case DRIVER_TYPE_USB: type_str = "USB"; break;
        case DRIVER_TYPE_BLOCK: type_str = "Block"; break;
        case DRIVER_TYPE_NET: type_str = "Net"; break;
        case DRIVER_TYPE_SOUND: type_str = "Sound"; break;
        case DRIVER_TYPE_GPU: type_str = "GPU"; break;
        case DRIVER_TYPE_INPUT: type_str = "Input"; break;
        default: type_str = "?"; break;
        }
        printk("  %c %-20s %-8s %s\n",
               drivers[i].loaded ? '*' : ' ',
               drivers[i].name, type_str,
               drivers[i].loaded ? "loaded" : "not loaded");
    }
    printk("\n");
}

/* ===== Login Manager ===== */

void login_manager_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Login Manager...\n");

    memset(users, 0, sizeof(users));
    memset(sessions, 0, sizeof(sessions));
    num_users = 0;

    /* Create default admin account */
    login_add_user("admin", "admin", 0, 1);
    login_add_user("guest", "", 1000, 0);

    printk_color(TERM_GREEN, "[BOOT] Login manager: %d users\n", num_users);
}

int login_add_user(const char *username, const char *password, uint32 uid, uint32 is_admin)
{
    if (num_users >= LOGIN_MAX_USERS || !username) return -1;

    user_account_t *u = &users[num_users++];
    strncpy(u->username, username, LOGIN_USERNAME_LEN - 1);
    strncpy(u->password_hash, password, LOGIN_PASSWORD_HASH - 1);
    u->uid = uid;
    u->gid = uid;
    u->is_admin = is_admin;
    u->is_active = 1;
    strcpy(u->home, "/home/");
    strcat(u->home, username);
    strcpy(u->shell, "/bin/sh");
    return 0;
}

int login_authenticate(const char *username, const char *password)
{
    if (!username || !password) return -1;

    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0 && users[i].is_active) {
            /* Simple string compare (in production: use hash) */
            if (strcmp(users[i].password_hash, password) == 0) {
                printk_color(TERM_GREEN, "[LOGIN] Authenticated: %s\n", username);
                return users[i].uid;
            }
        }
    }
    printk_color(TERM_RED, "[LOGIN] Authentication failed: %s\n", username);
    return -1;
}

user_session_t *login_create_session(const char *username)
{
    if (!username) return NULL;

    /* Find user */
    user_account_t *u = NULL;
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) { u = &users[i]; break; }
    }
    if (!u) return NULL;

    /* Find free session slot */
    for (int i = 0; i < LOGIN_MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            sessions[i].session_id = i + 1;
            strncpy(sessions[i].username, username, LOGIN_USERNAME_LEN - 1);
            sessions[i].uid = u->uid;
            sessions[i].login_time = rtc_get_time();
            sessions[i].last_activity = sessions[i].login_time;
            sessions[i].active = 1;

            /* Create desktop window for this session */
            sessions[i].desktop_window = wm_create_window("Desktop", 0, 0, FB_WIDTH, FB_HEIGHT, 0);
            if (sessions[i].desktop_window) {
                sessions[i].desktop_window->z_order = ZORDER_DESKTOP;
                sessions[i].desktop_window->bgcolor = 0xFF0078D7;
            }

            printk_color(TERM_GREEN, "[LOGIN] Session %d created for %s\n", i + 1, username);
            return &sessions[i];
        }
    }
    return NULL;
}

void login_destroy_session(uint32 session_id)
{
    for (int i = 0; i < LOGIN_MAX_SESSIONS; i++) {
        if (sessions[i].session_id == session_id && sessions[i].active) {
            sessions[i].active = 0;
            if (sessions[i].desktop_window) {
                wm_destroy_window(sessions[i].desktop_window);
                sessions[i].desktop_window = NULL;
            }
            printk_color(TERM_YELLOW, "[LOGIN] Session %d destroyed\n", session_id);
            return;
        }
    }
}

void login_list_sessions(void)
{
    printk_color(TERM_CYAN, "\n=== Active Sessions ===\n");
    for (int i = 0; i < LOGIN_MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            printk("  #%d: %s (uid=%d)\n", sessions[i].session_id,
                   sessions[i].username, sessions[i].uid);
        }
    }
    printk("\n");
}

void login_list_users(void)
{
    printk_color(TERM_CYAN, "\n=== Users ===\n");
    for (int i = 0; i < num_users; i++) {
        printk("  %s (uid=%d, %s, %s)\n",
               users[i].username, users[i].uid,
               users[i].is_admin ? "admin" : "user",
               users[i].is_active ? "active" : "disabled");
    }
    printk("\n");
}

void login_show_prompt(void)
{
    printk_color(TERM_CYAN, "\n========================================\n");
    printk_color(TERM_CYAN, "  torOS Login\n");
    printk_color(TERM_CYAN, "========================================\n");
    printk_color(TERM_WHITE, "  Username: ");
}

void login_lock_screen(void)
{
    login_active = 0;
    printk_color(TERM_CYAN, "[LOGIN] Screen locked\n");
}

void login_unlock_screen(const char *password)
{
    if (!password) return;
    for (int i = 0; i < LOGIN_MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            for (int u = 0; u < num_users; u++) {
                if (strcmp(users[u].username, sessions[i].username) == 0 &&
                    strcmp(users[u].password_hash, password) == 0) {
                    login_active = 1;
                    printk_color(TERM_GREEN, "[LOGIN] Screen unlocked\n");
                    return;
                }
            }
        }
    }
    printk_color(TERM_RED, "[LOGIN] Wrong password\n");
}

int login_change_password(const char *username, const char *old_pass, const char *new_pass)
{
    if (!username || !old_pass || !new_pass) return -1;
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0) {
            if (strcmp(users[i].password_hash, old_pass) == 0) {
                strncpy(users[i].password_hash, new_pass, LOGIN_PASSWORD_HASH - 1);
                printk_color(TERM_GREEN, "[LOGIN] Password changed: %s\n", username);
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

/* ===== Boot Stages ===== */

void boot_stage_drivers(void)
{
    printk_color(TERM_YELLOW, "\n[BOOT] === Stage 3: Driver Loading ===\n");
    driver_load_all();
}

void boot_stage_services(void)
{
    printk_color(TERM_YELLOW, "\n[BOOT] === Stage 4: Service Startup ===\n");
    init_run_services();
}

void boot_stage_userspace(void)
{
    printk_color(TERM_YELLOW, "\n[BOOT] === Stage 5: User Space ===\n");
    login_manager_init();
    printk_color(TERM_GREEN, "[BOOT] User space ready\n");
}

void boot_report(void)
{
    printk_color(TERM_CYAN, "\n========================================\n");
    printk_color(TERM_CYAN, "  torOS v0.4.0 Boot Report\n");
    printk_color(TERM_CYAN, "========================================\n");
    printk("  Services:  %d/%d running\n", num_services, num_services);
    printk("  Drivers:   %d loaded\n", num_drivers);
    printk("  Users:     %d accounts\n", num_users);
    printk("  Sessions:  %d active\n", 0);
    printk("  Memory:    %lu KB free\n", (get_free_pages() * PAGE_SIZE) / 1024);
    printk("  Network:   %s\n", net_is_up() ? "up" : "down");
    printk("  Display:   %dx%d\n", FB_WIDTH, FB_HEIGHT);
    printk("  Security:  PrivLevels=%d, ASLR=%s, NX=%s\n",
           NUM_PRIV_LEVELS, aslr_enabled() ? "on" : "off",
           nx_is_supported() ? "yes" : "no");
    printk_color(TERM_CYAN, "========================================\n\n");
}

/* ===== Main Boot Sequence ===== */

void toros_boot_sequence(void)
{
    printk_color(TERM_GREEN, "\n[BOOT] Starting torOS Boot Sequence...\n\n");

    /* Stage 1: ELF loader already done in entry.S */
    printk_color(TERM_YELLOW, "[BOOT] Stage 1-2: Kernel loaded (EL%d)\n",
                 (r_currentel() >> 2) & 3);

    /* Stage 3: Drivers */
    boot_stage_drivers();

    /* Stage 4: Services */
    boot_stage_services();

    /* Stage 5: User space */
    boot_stage_userspace();

    /* Report */
    boot_report();

    printk_color(TERM_GREEN, "[BOOT] torOS v0.4.0 is ready!\n\n");
}
