FROM euleros:econtainer AS gpu_device_plugin

# 复制编译好的 gpu-device-plugin 文件
COPY gpu-device-plugin /opt/xpu/bin/

RUN chmod 500 /opt/xpu/bin/gpu-device-plugin

# 当前容器创建的文件可能暴露到宿主机，从而与宿主机甚至其他容器的用户id碰撞
# 选择 10001 作为用户id/组id是为了避免与 useradd 自动生成的id碰撞
# 在本容器内新增用户时应当注意避免id碰撞
RUN echo "xpu:x:10001:10001:eXPUPoolService:/:/sbin/nologin" >> /etc/passwd \
    && echo "xpu:x:10001:" >> /etc/group \
    && echo "xpu:!::::::::" >> /etc/shadow \
    && chown xpu:xpu /opt/xpu/bin/gpu-device-plugin \
    && setcap CAP_DAC_OVERRIDE=ep /opt/xpu/bin/gpu-device-plugin

USER xpu:xpu

ENTRYPOINT ["/opt/xpu/bin/gpu-device-plugin"]