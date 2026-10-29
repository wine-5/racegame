# 安定版のEmscripten SDKをベースにする
FROM emscripten/emsdk:3.1.64

# 必要なパッケージの追加（基本ツールはすでに入っていますが、明示的に確保）
RUN apt-get update && apt-get install -y \
    cmake \
    git \
    python3 \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Linux ネイティブ版のビルドとデバッグに使うパッケージ
#   libsdl2-*-dev  : ネイティブ版がリンクする SDL2 本体・画像・音声ライブラリ
#   gdb            : VS Code（cppdbg）がネイティブ版のデバッグに使うデバッガ
#   libgl1-mesa-dri: ソフトウェア OpenGL。これが無いと SDL_CreateRenderer が
#                    「Couldn't find matching render driver」で失敗する
RUN apt-get update && apt-get install -y \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libgl1-mesa-dev \
    gdb \
    libgl1-mesa-dri \
    && rm -rf /var/lib/apt/lists/*

# Emscripten 版の SDL2 ポートを事前にビルドしておく
#   ヘッダーが cache/sysroot/include/SDL2 に入るので、初回ビルド前でも
#   IntelliSense（F12 の定義へ移動など）が SDL のヘッダーを見つけられる。
#   remoteUser（emscripten）がキャッシュに書き込めるよう所有者を戻す。
RUN embuilder build sdl2 sdl2_image:formats=png sdl2_mixer \
    && chown -R emscripten:emscripten /emsdk/upstream/emscripten/cache

# ~/.claude のボリュームを emscripten ユーザーの持ち物で作るため、先にフォルダを用意する。
# 無いと Docker が root の持ち物で作り、Claude Code が書き込めなくなる。
RUN mkdir -p /home/emscripten/.claude && chown emscripten:emscripten /home/emscripten/.claude

# 日本語のファイル名を扱えるようにする
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# コンテナ内の作業ディレクトリを設定
WORKDIR /workspace
