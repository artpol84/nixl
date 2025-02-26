#ifndef _NIXL_PARAMS_H
#define _NIXL_PARAMS_H

#include <string>
#include <cstdint>
#include "internal/backend_engine.h"
#include "nixl_types.h"

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
class nixlUcxInitParams : public nixlBackendInitParams {
    public:
        inline nixl_backend_t getType() const { return UCX; }
};

#endif
