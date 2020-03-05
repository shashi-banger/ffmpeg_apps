
BUILD commands

```
git clone https://github.com/shashi-banger/ffmpeg_apps.git

cd ffmpeg_apps

wget https://github.com/FFmpeg/FFmpeg/archive/n4.0.1.tar.gz

tar --skip-old-files -xvf n4.0.1.tar.gz --strip 1

#find ./ -maxdepth 1 ! -name "apps" ! -name n4.0.1.tar.gz ! -name Readme.md -exec rm -rf {} +

./ffmpeg_configure.sh
make all
sudo make install prefix=/ DESTDIR=/usr/local/amagi/
cd apps
mkdir build
cmake ../
```

Usage:

./av_to_es/av_to_es /home2/sb_media/scte_eg/scte_sample.ts /home2/sb_media/scte_eg/ out_file -s 0:2064:video:h264 -s 1:2068:audio:mp2 -s 2:500:data:scte35

/ts_muxer/ts_muxer /tmp/o.ts h264 /home2/sb_media/scte_eg/stream_video_00.es mp2 /home2/sb_media/scte_eg/stream_audio_01.es scte /home2/sb_media/scte_eg/stream_data_02.es
