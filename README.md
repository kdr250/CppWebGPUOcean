# C++ WebGPU Ocean
C++とWebGPUを使って流体シミュレーションを行う学習用レポジトリ。

## ビルド方法
### ネイティブの場合
[Build a project](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/how-to.md#build-a-project) を参考にして、ビルドして実行。

### Webの場合
1. `emcmake cmake -B build-web -G Ninja` を実行。
2. `cmake --build build-web` を実行。
3. `python -m http.server -d build-web` を実行してWebサーバー起動。
4. ブラウザで `http://localhost:8000/main.html` にアクセスして確認。

## 参考にしたURL
- [GitHub - WebGPU-Ocean](https://github.com/matsuoka-601/WebGPU-Ocean)
