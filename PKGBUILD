# Maintainer: iGerman00 <igerman at igerman dot cc>
pkgname=dolphin-waveform-git
pkgver=0
pkgrel=1
pkgdesc='KIO thumbnailer that renders audio album art with a waveform for Dolphin'
arch=('x86_64')
url='https://github.com/iGerman00/dolphin-waveform'
license=('GPL-3.0-only')
depends=('ffmpeg' 'kcolorscheme' 'kio' 'qt6-base')
makedepends=('cmake' 'extra-cmake-modules' 'git' 'kcoreaddons')
source=("git+$url.git")
b2sums=('SKIP')

_srcname=dolphin-waveform

pkgver() {
    cd "$srcdir/$_srcname"
    printf 'r%s.g%s' "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cmake -S "$srcdir/$_srcname" -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_SHARED_LIBS=ON
    cmake --build build --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install build
    install -Dm644 "$srcdir/$_srcname/LICENSE" \
        "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
