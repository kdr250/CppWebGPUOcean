#include <iostream>
#include "Application.h"

int main(int argc, char* argv[])
{
    Application app;
    if (!app.Initialize())
    {
        return EXIT_FAILURE;
    }

    app.RunLoop();

    app.Shutdown();
    return 0;
}
