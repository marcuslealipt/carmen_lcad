# syntax=docker/dockerfile:1
ARG UBUNTU_IMAGE=ubuntu:26.04
FROM ${UBUNTU_IMAGE} AS dependencies
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_JOBS=2
ENV LANG=C.UTF-8 LC_ALL=C.UTF-8 \
    CARMEN_HOME=/opt/carmen \
    PATH=/opt/carmen/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/opt/carmen/lib:/opt/carmen/sharedlib/darknet/lib:/usr/local/lib:/opt/carmen/sharedlib/libtf/src:/opt/carmen/sharedlib/libcarmodel:/opt/carmen/sharedlib/libcontrol:/opt/carmen/sharedlib/OpenJAUS/libopenJaus/lib:/opt/carmen/sharedlib/OpenJAUS/libjaus/lib:/opt/carmen/sharedlib/OpenJAUS/ojTorc/lib:/opt/carmen/sharedlib/OpenJAUS/ojIARASim/lib \
    DARKNET_HOME=/opt/carmen/sharedlib/darknet \
    CENTRALHOST=localhost
COPY docker/apt-packages.txt /tmp/apt-packages.txt
RUN test "$(dpkg --print-architecture)" = amd64 \
    && apt-get update \
    && xargs -r apt-get install -y --no-install-recommends < /tmp/apt-packages.txt \
    && rm -rf /var/lib/apt/lists/*
COPY docker/scripts/install-gtkglext.sh /tmp/install-gtkglext.sh
RUN bash /tmp/install-gtkglext.sh "${BUILD_JOBS}"

FROM dependencies AS source
WORKDIR /opt/carmen
# Contexto na raiz, filtrado por /.dockerignore. Headers e prebuilt não são descartados.
COPY src/ src/
COPY include/ include/
COPY sharedlib/ sharedlib/
COPY prebuilt/ubuntu26-x86_64/ prebuilt/ubuntu26-x86_64/
COPY bin/ bin/
COPY data/ data/
COPY docker/ docker/

FROM source AS build
ARG BUILD_JOBS=2
ARG SOURCE_REVISION=unknown
RUN mkdir -p /opt/carmen/build-logs /opt/carmen/lib /opt/carmen/man \
    && bash docker/scripts/build.sh "${BUILD_JOBS}" 2>&1 | tee build-logs/build.log
RUN printf '%s\n' "${SOURCE_REVISION}" > build-logs/source-revision.txt \
    && dpkg-query -W > build-logs/dpkg-packages.tsv \
    && python3 docker/scripts/prepare.py --root /opt/carmen --output /opt/carmen/docker/generated \
    && bash docker/scripts/check-binaries.sh

FROM build AS runtime
ARG LOCAL_UID=1000
ARG LOCAL_GID=1000
# UID numérico evita conflito com o usuário ubuntu presente em algumas imagens-base.
RUN mkdir -p /home/carmen /run/carmen /logs /dados \
    && chown -R "${LOCAL_UID}:${LOCAL_GID}" /home/carmen /run/carmen /logs /dados /opt/carmen
ENV HOME=/home/carmen LIBGL_ALWAYS_SOFTWARE=1 QT_X11_NO_MITSHM=1
USER ${LOCAL_UID}:${LOCAL_GID}
WORKDIR /opt/carmen/bin
STOPSIGNAL SIGINT
ENTRYPOINT ["python3", "/opt/carmen/docker/scripts/run.py"]
CMD ["--mode", "core"]
