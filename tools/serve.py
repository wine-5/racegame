"""ブラウザ版の開発用サーバー。

python3 -m http.server と同じくワークスペースを配信するが、ブラウザにキャッシュさせない。
ビルドし直したあと、ふつうに再読み込みするだけで新しい .wasm が読まれる。
"""

import http.server
import sys


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    """毎回ファイルを読み直させるヘッダーを付けて返す"""

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    http.server.test(HandlerClass=NoCacheHandler, port=port)


if __name__ == "__main__":
    main()
