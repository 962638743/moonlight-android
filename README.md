# 在 rexq57/moonlight-android 基础上的优化版
* 启用了 华为麒麟系列SOC的低延迟解码功能
* 添加了 2400x1080分辨率支持。 

# 说明
该项目针对官方代码进行了大量优化，主要改进如下：

* 增加了4K (21:9 3840x1644) 和 2K (21:9 2560x1096) 分辨率
* 提高渲染效率和实时性，画面稳定
* 帧率统计信息更准确

由于使用了MediaCodec NDK，API要求 >= 21（android系统版本5.0）

## 如何更改分辨率

在Nvidia控制面板中，增加自定义分辨率，并设置电脑分辨率。

注意：只有一部分游戏支持21:9的超宽屏。


## 如何搭建5G 4K 100Mbps流畅串流环境

1、尽可能清空5G通道，能走有线的设备走有线

2、低速率设备放到2.4G，专门为游戏手机提供一个独立的5G通道

3、使用一个高端路由器，例如华硕RT-AX92U



# Moonlight Android

[![Travis CI Status](https://travis-ci.org/moonlight-stream/moonlight-android.svg?branch=master)](https://travis-ci.org/moonlight-stream/moonlight-android)

[Moonlight for Android](https://moonlight-stream.org) is an open source implementation of NVIDIA's GameStream, as used by the NVIDIA Shield.

Moonlight for Android will allow you to stream your full collection of games from your Windows PC to your Android device,
whether in your own home or over the internet.

Moonlight also has a [PC client](https://github.com/moonlight-stream/moonlight-qt) and [iOS/tvOS client](https://github.com/moonlight-stream/moonlight-ios).

Check out [the Moonlight wiki](https://github.com/moonlight-stream/moonlight-docs/wiki) for more detailed project information, setup guide, or troubleshooting steps.

## Downloads
* [Google Play Store](https://play.google.com/store/apps/details?id=com.limelight)
* [Amazon App Store](https://www.amazon.com/gp/product/B00JK4MFN2)
* [F-Droid](https://f-droid.org/packages/com.limelight)
* [APK](https://github.com/moonlight-stream/moonlight-android/releases)

## Building
* Install Android Studio and the Android NDK
* Run ‘git submodule update --init --recursive’ from within moonlight-android/
* In moonlight-android/, create a file called ‘local.properties’. Add an ‘ndk.dir=’ property to the local.properties file and set it equal to your NDK directory.
* Build the APK using Android Studio or gradle

## Authors

* [Cameron Gutman](https://github.com/cgutman)  
* [Diego Waxemberg](https://github.com/dwaxemberg)  
* [Aaron Neyer](https://github.com/Aaronneyer)  
* [Andrew Hennessy](https://github.com/yetanothername)

Moonlight is the work of students at [Case Western](http://case.edu) and was
started as a project at [MHacks](http://mhacks.org).
