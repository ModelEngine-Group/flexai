FROM euleros:econtainer

# 复制编译好的 npu_client 文件
COPY ./NPU_client /root

# /root 目录下除了项目输出文件外没有非隐藏文件，以小数点(.)开头的隐藏文件不会被通配符(*)匹配
RUN chmod 500 /root/*

USER root
ENTRYPOINT ["/root/acl-client-update.sh"]