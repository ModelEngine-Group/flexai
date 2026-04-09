#!/bin/bash
# Copyright Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# hwp7s签名用于CMC B版本发布
set -e

pkg_path=$1
current_dir=$(
    cd "$(dirname "$0")" || exit 1
    pwd
)
workspace=$(dirname "${current_dir}")
signature_jar=$(find /opt/buildtools/ -name signature.jar)

function gen_signature_xml() {
    cat << EOF > "${workspace}"/CIConfig.xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- 由产品CI配置此文件，供私有构建、团队构建、发布构建等各级工程共享 -->
<signtasks>
  <signtask name="cms_sign">
    <alias>CMS_G5_Test_Sign_RSA3072PSS_CN_20220505_HUAWEI</alias>
    <timestampalias>CMS_G5_Test_TSA_RSA3072PSS_CN_20220505_HUAWEI</timestampalias>
    <fileset path="${pkg_path}">
      <include>**/*.zip</include>
      <include>**/*.iso</include>
      <include>**/*.tar</include>
      <include>**/*.tar.gz</include>
      <include>**/*.tgz</include>
    </fileset>
    <crlfile>${pkg_path}/crldata.crl/</crlfile>
    <hashtype>2</hashtype>
    <proxylist>10.29.154.209:12056</proxylist>
    <signaturestandard>5</signaturestandard>
    <productlineid>049944</productlineid>
    <versionid>261181132</versionid>
    <padmode>1</padmode>
  </signtask>
</signtasks>
EOF
}

gen_signature_xml

# sign
if ! java -jar "${signature_jar}" "${workspace}"/CIConfig.xml; then
    echo "signature execute failed. exit."
    exit 1
fi