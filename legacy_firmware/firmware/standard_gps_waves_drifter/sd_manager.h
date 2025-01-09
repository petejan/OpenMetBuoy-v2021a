#ifndef SD_MANAGER
#define SD_MANAGER

#include "Arduino.h"
#include "SdFat.h"

#include "params.h"


class SdManager
{
    public:
        void start(void);
        int write(const char[], void *, uint32_t);
        void end(void);
    private:
        SdFat sd;
        SdFile file; // Log file.
        bool sd_card_open = false;
        int GetNextIndex(void);
};

extern SdManager sd_manager;

#endif