/*
 * torOS Boot & Init Subsystem Header
 * Multi-stage boot, init system, driver auto-loading, login manager
 */

#ifndef _INIT_H
#define _INIT_H

#include "toros.h"

/* ===== Boot Stages ===== */
#define BOOT_STAGE_ELF      1
#define BOOT_STAGE_KERNEL   2
#define BOOT_STAGE_DRIVERS  3
#define BOOT_STAGE_SERVICES 4
#define BOOT_STAGE_USERSPACE 5

/* ===== Init System ===== */
#define INIT_MAX_SERVICES   32
#define INIT_MAX_DEPENDENCIES 8
#define INIT_SERVICE_NAME_LEN 32
#define INIT_CMD_LEN        128

typedef enum {
    SERVICE_STOPPED,
    SERVICE_STARTING,
    SERVICE_RUNNING,
    SERVICE_STOPPING,
    SERVICE_FAILED,
    SERVICE_RESTARTING
} service_state_t;

typedef struct {
    char name[INIT_SERVICE_NAME_LEN];
    char command[INIT_CMD_LEN];
    char description[64];
    service_state_t state;
    uint32 pid;
    uint32 auto_start;
    uint32 restart_on_crash;
    uint32 restart_count;
    uint32 max_restarts;
    uint64 start_time;
    char dependencies[INIT_MAX_DEPENDENCIES][INIT_SERVICE_NAME_LEN];
    int num_dependencies;
    int (*init_func)(void);
    int initialized;
} service_t;

/* ===== Driver Loading ===== */
#define DRIVER_MAX          32
#define DRIVER_NAME_LEN     32

typedef enum {
    DRIVER_TYPE_PCI,
    DRIVER_TYPE_USB,
    DRIVER_TYPE_BLOCK,
    DRIVER_TYPE_NET,
    DRIVER_TYPE_SOUND,
    DRIVER_TYPE_GPU,
    DRIVER_TYPE_INPUT
} driver_type_t;

typedef struct {
    char name[DRIVER_NAME_LEN];
    driver_type_t type;
    uint32 vendor_id;
    uint32 device_id;
    uint32 class_code;
    int (*probe)(void);
    int (*init)(void);
    void (*shutdown)(void);
    int loaded;
    int (*match_pci)(uint32 vendor, uint32 device, uint32 class_code);
} driver_t;

/* ===== Login Manager ===== */
#define LOGIN_MAX_USERS     16
#define LOGIN_USERNAME_LEN  32
#define LOGIN_PASSWORD_HASH 64
#define LOGIN_MAX_SESSIONS  4

typedef struct {
    char username[LOGIN_USERNAME_LEN];
    char password_hash[LOGIN_PASSWORD_HASH];
    uint32 uid;
    uint32 gid;
    char home[128];
    char shell[32];
    uint32 is_admin;
    uint32 is_active;
} user_account_t;

typedef struct {
    uint32 session_id;
    char username[LOGIN_USERNAME_LEN];
    uint32 uid;
    uint64 login_time;
    uint64 last_activity;
    window_t *desktop_window;
    int active;
} user_session_t;

/* ===== Init API ===== */
void init_system_init(void);
void init_run_services(void);
int init_register_service(const char *name, const char *command,
                          const char *description, uint32 auto_start,
                          int (*init_func)(void));
int init_start_service(const char *name);
int init_stop_service(const char *name);
void init_service_status(const char *name);
void init_list_services(void);

/* Driver Loading */
void driver_init(void);
int driver_register(const char *name, driver_type_t type,
                    int (*probe)(void), int (*init_func)(void), void (*shutdown_func)(void));
int driver_load_by_type(driver_type_t type);
int driver_load_all(void);
void driver_list(void);

/* Login Manager */
void login_manager_init(void);
int login_add_user(const char *username, const char *password, uint32 uid, uint32 is_admin);
int login_authenticate(const char *username, const char *password);
user_session_t *login_create_session(const char *username);
void login_destroy_session(uint32 session_id);
void login_list_sessions(void);
void login_list_users(void);
void login_lock_screen(void);
void login_unlock_screen(const char *password);
int login_change_password(const char *username, const char *old_pass, const char *new_pass);
void login_show_prompt(void);

/* Boot stages */
void boot_stage_drivers(void);
void boot_stage_services(void);
void boot_stage_userspace(void);
void boot_report(void);

#endif
