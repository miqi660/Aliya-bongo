# GLEW 2.2.0

来源：SDK 自带 `Samples/OpenGL/thirdParty/scripts/setup_glew_glfw.bat` 指定的官方版本。
下载地址：https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
归档 SHA-256：a9046a913774395a095edcc0b0ac2d81c3aacca61787b39839b941e9be14e0d4

仅保留原始 include/GL、src/glew.c 和 LICENSE.txt；不修改上游源码。
由 native/build.rs 静态编译，构建无需下载。Cubism 和 stb 继续使用指定 SDK。
