
#include "test.h"

#include <rvk/rvk.h>

i32 main() {
    {
        Test test{"basic"_v};

        rvk::Config config{
            .layers = {},
            .instance_extensions = {},
            .validation = true,
            .frames_in_flight = 2,
        };
        if(!rvk::startup(config)) {
            die("Failed to startup rvk.");
        }
        rvk::shutdown();
    }
    Profile::finalize();
    return 0;
}
