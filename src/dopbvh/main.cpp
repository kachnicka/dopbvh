#include "Application.h"
#include <berries/lib_helper/spdlog.h>

int main(int argc, char* argv[])
{
    berry::Log::Init();
    berry::log::timer("Application start", glfwGetTime());

    Application app { argc, argv };
    return app.Run();
}
