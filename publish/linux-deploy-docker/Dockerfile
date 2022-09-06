FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y binutils libgssapi-krb5-2 libgl1-mesa-glx libglib2.0-dev libfontconfig-dev \
        libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 libxcb-xkb1 libxkbcommon-x11-0 libxcb-xinerama0\
        libdbus-1-3 libegl1&& \
    apt-get clean -y && \   
    rm -rf \
    /var/cache/debconf/* \
    /var/lib/apt/lists/* \
    /var/log/* \
    /var/tmp/* \
    && rm -rf /tmp/*