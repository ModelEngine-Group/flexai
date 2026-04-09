FROM euleros:econtainer AS npu_device_plugin

# 复制编译好的 npu-device-plugin 文件
COPY npu-device-plugin /opt/xpu/bin/

RUN chmod 500 /opt/xpu/bin/npu-device-plugin

# 运行时必须使用root用户，否则会报错。
# 使用非root权限用户运行会导致npu-device-plugin运行时加载libdcml.so文件失败。原因如下：
# 1. ascended-npu-exporter在加载libdcml.so动态库时会分别尝试在 LD_LIBRARY_PATH 环境变量指定的目录和 .ldconfig 缓存项目中查找动态文件。
# 2. 构建容器镜像时，如果指定的运行用户是非root权限用户，我们需要对npu-device-plugin二进制文件进行CAP_DAC_OVERRIDE=ep的授权操作。
#    由于对npu-device-plugin的授权，导致程序无法获取到包含libdcml.so文件路径的"LD_LIBRARY_PATH"环境变量。
#    程序无法通过环境变量设置的路径找到libdcml.so，这将导致在初始化的时候无法找到so文件。
# 3. 当环境变量中找不到libdcml.so文件时，程序会执行"ldconfig"命令并获取缓存内容，但是libdcml.so文件只在运行环境中存在。
#    构建环境下的ldconfig无法识别该文件，因此无法将其写入缓存（更新缓存内容需要使用root权限）。
#    因此构建容器镜像时，如果指定的运行用户是非root权限用户，容器内部无法使用ldconfig更新缓存。
USER root

ENTRYPOINT ["/opt/xpu/bin/npu-device-plugin"]