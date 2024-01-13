
#include "test.h"

#include <rvk/rvk.h>

i32 main() {
    {
        Test test{"basic"_v};

        rvk::Config config{
            .frames_in_flight = 2,
            .use_validation = true,
            .instance_extensions = {},
        };
        if(!rvk::startup(config)) {
            die("Failed to startup rvk.");
        }
        rvk::shutdown();
    }
    Profile::finalize();
    return 0;
}
