#include "ldde/core/application.hpp"
#include "ldde/core/logging.hpp"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        ldde::core::Application app;

        ldde::core::Status init_status = app.initialize(argc, argv);
        if (init_status.is_error()) {
            std::cerr << "Initialization failed: " << init_status.to_string() << "\n";
            return 1;
        }

        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal unhandled exception: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown exception caught in main()\n";
        return 1;
    }
}

