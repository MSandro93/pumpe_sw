
#define max_nbr_appointsments 7
#define one_tick_in_mills 1000

struct appointment
{
    int hour = -1;
    int min = -1;
    void (*func_ptr)() = nullptr;
    bool pending_today = true;
    char* description;
};


void scheuduler_init();
int scheuduler_addAppointment(int h_, int m_, void (*fp_)(), const char* desc_);
int scheuduler_trigger(const char* desc_);
int scheuduler_overwrite(const char* desc_, int h_, int m_, void (*fp_)(), const char* desc_new);
int scheuduler_setActive(const char* desc_, bool active_);
void scheuduler_loop();
void scheuduler_reactivateAll(void);
appointment* scheuduler_getAppointment(const char* desc_);