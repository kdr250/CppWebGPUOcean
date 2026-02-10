# C++ WebGPU Ocean
C++とWebGPUを使って流体シミュレーションを学習するためのレポジトリ。<br>
[Demo](https://kdr250.github.io/CppWebGPUOcean/)

## ビルド方法
### ネイティブの場合
1. `cmake -B build -G Ninja`
2. `cmake --build build`
3. `cd build`
4. `./main`

### Webの場合
1. `emcmake cmake -B build-web -G Ninja`
2. `cmake --build build-web`
3. `python -m http.server -d build-web`
4. Webブラウザで `http://localhost:8000/main.html` にアクセス

## 参考にしたURL
- [GitHub - WebGPU-Ocean](https://github.com/matsuoka-601/WebGPU-Ocean)
- [Zenn - WebGPU で実装したリアルタイム 3D 流体シミュレーションの紹介](https://zenn.dev/sparkle/articles/217cc2bb44fd9e)
