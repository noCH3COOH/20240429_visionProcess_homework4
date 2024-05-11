# 视频压缩码率与视频质量的关系
21009102148 黄陈智博

## 1. 实验目标

使用现有编解码器将原始视频进行编码、解码得到重建视频，分析压缩码率与视频质量的关系

## 2. 实现方案

![alt text](programFlow.png)

总共使用如下码率
```C

long input_code_rate[] = {
    10*1024,    // 10Kbps
    100*1024,    // 100Kbps
    200*1024,    // 300Kbps
    300*1024,    // 300Kbps
    400*1024,    // 300Kbps
    500*1024,    // 500Kbps
    600*1024,    // 300Kbps
    700*1024,    // 700Kbps
    800*1024,    // 300Kbps
    900*1024,    // 900Kbps
    1024*1024,    // 1Mbps
};
```

采用如下命令对视频码率转换
```shell
ffmpeg -i test.mp4 -c:a copy -b:v "X" -y out"X".mp4
```
`"X"` 为输入码率

## 3. 实现结果

### 3.1 输入视频信息

```shell
noch3cooh@noch3cooh-JDBook:~/project/20240307_visionProcess/decode_encode$ ffmpeg -i test.mp4
ffmpeg version 4.4.2-0ubuntu0.22.04.1 Copyright (c) 2000-2021 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.2.0-19ubuntu1)
  configuration: --prefix=/usr --extra-version=0ubuntu0.22.04.1 --toolchain=hardened --libdir=/usr/lib/x86_64-linux-gnu --incdir=/usr/include/x86_64-linux-gnu --arch=amd64 --enable-gpl --disable-stripping --enable-gnutls --enable-ladspa --enable-libaom --enable-libass --enable-libbluray --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libgme --enable-libgsm --enable-libjack --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-libpulse --enable-librabbitmq --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libsrt --enable-libssh --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-libzmq --enable-libzvbi --enable-lv2 --enable-omx --enable-openal --enable-opencl --enable-opengl --enable-sdl2 --enable-pocketsphinx --enable-librsvg --enable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-libx264 --enable-shared
  libavutil      56. 70.100 / 56. 70.100
  libavcodec     58.134.100 / 58.134.100
  libavformat    58. 76.100 / 58. 76.100
  libavdevice    58. 13.100 / 58. 13.100
  libavfilter     7.110.100 /  7.110.100
  libswscale      5.  9.100 /  5.  9.100
  libswresample   3.  9.100 /  3.  9.100
  libpostproc    55.  9.100 / 55.  9.100
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'test.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    encoder         : Lavf59.6.100
  Duration: 00:02:21.18, start: 0.000000, bitrate: 4681 kb/s
  Stream #0:0(eng): Video: h264 (Main) (avc1 / 0x31637661), yuv420p(tv, bt709), 1920x1080 [SAR 1:1 DAR 16:9], 4544 kb/s, 60 fps, 60 tbr, 15360 tbn, 120 tbc (default)
    Metadata:
      handler_name    : ?Mainconcept Video Media Handler
      vendor_id       : [0][0][0][0]
  Stream #0:1(eng): Audio: aac (LC) (mp4a / 0x6134706D), 48000 Hz, stereo, fltp, 128 kb/s (default)
    Metadata:
      handler_name    : #Mainconcept MP4 Sound Media Handler
      vendor_id       : [0][0][0][0]
At least one output file must be specified

```

可得码率约为20Mbps。通过视频属性可得原视频大小为78.8MB。

### 3.2 输出视频结果

* 10kbps

![alt text](10K_1.png)

![alt text](10K_2.png)

可见视频码率10Kbps下由于码率过低，视频开头出现完全没有画面的情况，并且在视频中间有画面的部分也模糊不清。

* 100kbps

![alt text](100K.png)

此码率下避免了开头完全没有画面的情况，但视频全部分依然模糊不清，无法观看。

* 200Kbps

![alt text](200K_1.png)

![alt text](200K_2.png)

此码率下全时长均有画面，但开头依然出现画面模糊的情况，在之后视频较为清晰。由于在开始几秒后，视频播放较为清晰，所以认为200Kbps下已能清晰地播放视频，只是因为播放器编解码器、显卡渲染等原因未能做到清晰播放。此时导出视频大小为5.8MB，压缩比达到了$92.6\%$

## 4. 收获心得

本次实验控制了码率为变量对视频质量进行分析。通过实验，我知道了视频的码率具有一定的边际效应，并且在较低的码率下能够做到损失少量质量地对视频进行大幅度压缩。让视频在速率较低的通信网络中也能够进行传输。
