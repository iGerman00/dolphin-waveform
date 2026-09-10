# Maintainer: iGerman00
pkgname=dolphin-waveform
pkgver=0.1.0
pkgrel=1
pkgdesc='KIO thumbnailer that renders audio album art with a waveform for Dolphin'
arch=('x86_64')
url='https://github.com/iGerman00/dolphin-waveform'
license=('GPL-2.0-or-later')
depends=('ffmpeg' 'kcolorscheme' 'kio' 'qt6-base')
makedepends=('cmake' 'git')
source=("git+$url.git#commit=4884de6d306526d0230db124575aafe0bfa34f33")
b2sums=('SKIP')

build() {
    cmake -S "$srcdir/$pkgname" -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}
