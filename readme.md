# FFmpeg Linux 环境下构建

## 构建依赖

### nasm 汇编器

Netwide Assembler (简称NASM) 是一款基于英特尔 x86 架构的汇编与反汇编工具。NASM 被认为是 Linux 平台上最受欢迎的汇编工具之一。
注意，NASM 是 x86 平台汇编器，不需要交叉编译。若是 arm 等其他平台，交叉编译工具链中包含有对应的汇编器，则交叉编译 ffmpeg 时需要 --disable-x86asm 选项。

NASM 官网：https://www.nasm.us/

下载源码

```
wget https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz
tar zxvf nasm-2.15.05.tar.gz
```

构建&安装

```
./configure
make -j 2
sudo make install  
```

> 默认安装在 /usr/local 目录下

### x264

FFmpeg 的工程中实现了 H264 的解码，但未实现 H264 的编码。x264 是开源的 H264 编码器。

项目官网：https://www.videolan.org/developers/x264.html

下载源码

```
git clone https://code.videolan.org/videolan/x264.git
```

构建&安装

```
./configure --enable-shared
make -j 2
sudo make install
```

> 默认安装在 /usr/local 目录下

### x265

FFmpeg 工程中实现了 H265 解码器，但无 H265 编码器。因此需要安装第三方编码器 

x265 官网：http://www.x265.org/

下载地址：https://www.videolan.org/developers/x265.html

下载源码

```
git clone https://bitbucket.org/multicoreware/x265_git.git
```

构建&安装

```
cd build/linux
./make-Makefiles.bash
make -j 2
sudo make install
```

> 默认安装在 /usr/local 目录下

### libmp3lame

libmp3lame 是开源的 mp3 编码器。

libmp3lame 官网：http://lame.sourceforge.net/

构建&安装

```
./configure --enable-static=no --enable-nasm
make -j 2
sudo make install
```

## 构建 FFmpeg

```
git clone https://git.ffmpeg.org/ffmpeg.git
git checkout tags/n4.2.2 -b n4.2.2

./configure  --enable-shared --enable-gpl --enable-pthreads --enable-libx264 --enable-libx265 --enable-libmp3lame

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

## FAQ

编译ffmpeg，运行 ./configure --enable-libx265 ...出现如下错误提示：

x265 not found using pkg-config

解决方法：

```
export PKG_CONFIG_PATH=$HOME/lib/pkgconfig:$PKG_CONFIG_PATH
```
