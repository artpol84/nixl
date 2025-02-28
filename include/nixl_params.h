#ifndef _NIXL_PARAMS_H
#define _NIXL_PARAMS_H

#include <string>
#include <cstdint>
#include "internal/backend_engine.h"
#include "nixl_types.h"
#include "utils/sys/nixl_time.h"

// Per Agent configuration information, such as if progress thread should be used.
// Other configs such as assigned IP/port or device access can be added.
class nixlAgentConfig {
    private:
        bool useProgThread;

    public:
        // Important configs such as useProgThread must be given and can't be changed.
        nixlAgentConfig(const bool useProgThread) {
            this->useProgThread = useProgThread;
        }
        nixlAgentConfig(const nixlAgentConfig &cfg) = default;
        ~nixlAgentConfig() = default;

    friend class nixlAgent;
};

// Example backend initialization data for UCX.
// For now UCX autodetects devices, later might add hints.
class nixlUcxInitParams : public nixlBackendInitParams 
{
    public:

        /* 
         * Restrict the list of used UCX devices
         * empty vector instructs UCX to use all
         * supported devices found on the node
         */
        std::vector<std::string> devices;

        /* 
         * Progress frequency knob (in us)
         * The progress thread is calling sched_yield to avoid blocking a core
         * If pthrDelay time is less than sched_yield time - option has no effect
         * Otherwise pthread will be calling sched_yield untill the specified
         * amount of time has past.
         */
        nixlTime::us_t pthrDelay;

        nixlUcxInitParams() : nixlBackendInitParams()  {
            pthrDelay = 0;
        }

        inline nixl_backend_t getType() const { return UCX; }

};

#endif
