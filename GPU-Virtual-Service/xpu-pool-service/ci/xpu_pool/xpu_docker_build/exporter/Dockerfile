FROM euleros:econtainer AS xpu_exporter

COPY ./euler/ /opt/euler/
# 复制编译好的 xpu_exporter 文件
COPY xpu-exporter /opt/xpu/bin/

RUN echo "[dvd]" >> /etc/yum.repos.d/dvd.repo \
    && echo "name=install dvd" >> /etc/yum.repos.d/dvd.repo \
    && echo "baseurl=file:///opt/euler" >> /etc/yum.repos.d/dvd.repo \
    && echo "enabled=1" >> /etc/yum.repos.d/dvd.repo \
    && echo "gpgcheck=0" >> /etc/yum.repos.d/dvd.repo \
    && yum clean all && yum makecache \
    && yum -y install openssl \
    && rm -rf /opt/euler \
    && rm -f /etc/yum.repos.d/dvd.repo

RUN chmod 500 /opt/xpu/bin/xpu-exporter

# 当前容器创建的文件可能暴露到宿主机，从而与宿主机甚至其他容器的用户id碰撞
# 选择 10001 作为用户id/组id是为了避免与 useradd 自动生成的id碰撞
# 在本容器内新增用户时应当注意避免id碰撞
RUN echo "xpu:x:10001:10001:eXPUPoolService:/:/sbin/nologin" >> /etc/passwd \
    && echo "xpu:x:10001:" >> /etc/group \
    && echo "xpu:!:::::::" >> /etc/shadow \
    && chown xpu:xpu /opt/xpu/bin/xpu-exporter \
    && setcap CAP_DAC_OVERRIDE=ep /opt/xpu/bin/xpu-exporter

USER xpu:xpu

ENTRYPOINT ["/opt/xpu/bin/xpu-exporter"]