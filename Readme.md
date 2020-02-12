
BUILD
git clone https://github.com/shashi-banger/ffmpeg_apps.git

cd ffmpeg_apps

wget https://github.com/FFmpeg/FFmpeg/archive/n4.0.1.tar.gz

tar --skip-old-files -xvf n4.0.1.tar.gz --strip 1

#find ./ -maxdepth 1 ! -name "apps" ! -name n4.0.1.tar.gz ! -name Readme.md -exec rm -rf {} +

./ffmpeg_configure.sh
make all
make install prefix=/ DESTDIR=ffmpeg_bin
cd apps
mkdir build
cmake ../
