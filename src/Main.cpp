// コンポジションルート。アダプターを選んで組み立て、動かすだけ。
#include "adapters/file/TrackFile.hpp"
#include "adapters/headless/SelfTest.hpp"
#include "adapters/sdl/SdlApp.hpp"
#include "application/GameSession.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    bool selfTest{argc > 1 && std::strcmp(argv[1], "--self-test") == 0};

    application::GameSession session;
    adapters::file::TrackFile trackFile{adapters::file::TrackFile::findDefaultPath()};
    if (!session.start(trackFile))
    {
        std::fprintf(stderr, "コースを読めません: %s\n", trackFile.path().c_str());
        return 1;
    }

    if (selfTest)
    {
        return adapters::headless::runSelfTest(session.track());
    }

    adapters::sdl::SdlApp app{session};
    // 開発用: --capture <file.png> [frame] [--drift] で、そのフレームの画面を保存して終わる
    adapters::sdl::CaptureOptions capture;
    for (int i{1}; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
        {
            capture.path = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                capture.frame = std::atoi(argv[++i]);
            }
        }
        else if (std::strcmp(argv[i], "--drift") == 0)
        {
            capture.autoDrive = true;
        }
    }
    app.setCapture(capture);
    if (!app.init())
    {
        return 1;
    }
    app.run();
    return 0;
}
