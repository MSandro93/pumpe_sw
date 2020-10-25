#include <stdint.h>

#define max_nbr_appointsments 7
#define one_tick_in_mills 1000

struct appointment
{
    int hour = -1;
    int min = -1;
    void (*func_ptr)() = nullptr;
    bool pending_today = true;
    char* description;
    bool active = true;
};


void scheduler_init();
int scheduler_addAppointment(int h_, int m_, void (*fp_)(), const char* desc_);
int scheduler_trigger(const char* desc_);
int scheduler_overwrite(const char* desc_, int h_, int m_, void (*fp_)(), const char* desc_new);
int scheduler_setActive(const char* desc_, bool active_);
int scheduler_setPendingToday(const char* desc_, bool pending_);
bool scheudler_getActive(const char* desc_);
void scheduler_loop();
void scheduler_setAllToPendingToday(void);
appointment* scheduler_getAppointment(const char* desc_);
void scheduler_print_all_appointments(char* str);
void sort_appointments();
void scheduler_reset_real_watering_time_today(void);