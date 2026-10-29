// コンポジションルート。アダプターを選んで組み立て、動かすだけ。
#include "adapters/course/PortCourse.hpp"
#include "adapters/course/PracticeCircle.hpp"
#include "adapters/file/TrackFile.hpp"
#include "adapters/headless/SelfTest.hpp"
#include "adapters/sdl/SdlApp.hpp"
#include "application/GameSession.hpp"

#include <strings.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv)
{
    bool selfTest{argc > 1 && std::strcmp(argv[1], "--self-test") == 0};

    application::GameSession session;
    adapters::file::TrackFile suzuka{adapters::file::TrackFile::findDefaultPath()};
    if (selfTest)
    {
        // セルフテストは、全長などの基準がある鈴鹿で走らせる
        if (!session.start(suzuka))
        {
            std::fprintf(stderr, "コースを読めません: %s\n", suzuka.path().c_str());
            return 1;
        }
        return adapters::headless::runSelfTest(session.track());
    }

    // 数字キーの 1 から順に割り当てる
    adapters::course::PortCourse port;
    adapters::course::PracticeCircle skidpad;
    std::vector<adapters::sdl::Stage> stages{
        {"PORT", &port, adapters::gl::Scenery::Port},
        {"SKIDPAD", &skidpad, adapters::gl::Scenery::Skidpad},
        {"SUZUKA", &suzuka, adapters::gl::Scenery::Circuit},
    };
    // --stage <名前> で最初のステージを選べる。既定は港
    int firstStage{};
    for (int i{1}; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--stage") != 0)
        {
            continue;
        }
        for (std::size_t k{}; k < stages.size(); ++k)
        {
            if (strcasecmp(argv[i + 1], stages[k].name.c_str()) == 0)
            {
                firstStage = static_cast<int>(k);
            }
        }
    }

    adapters::sdl::SdlApp app{session, stages, firstStage};
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
