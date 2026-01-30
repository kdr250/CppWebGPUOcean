#include "Application.h"

int main(int argc, char* argv[])
{
    Application app;
    if (!app.Initialize())
    {
        return 1;
    }

    app.RunLoop();

    return 0;
}
