module huawei.com/xpu-exporter

go 1.22.1

replace (
	google.golang.org/grpc => google.golang.org/grpc v1.57.2
	huawei.com/vxpu-device-plugin => ../GPU-device-plugin
)

require (
	github.com/agiledragon/gomonkey/v2 v2.8.0
	github.com/prometheus/client_golang v1.16.0
	github.com/stretchr/testify v1.9.0
	google.golang.org/grpc v1.65.0
	google.golang.org/protobuf v1.34.2
	huawei.com/vxpu-device-plugin v0.0.0-00010101000000-000000000000
)

require (
	github.com/beorn7/perks v1.0.1 // indirect
	github.com/cespare/xxhash/v2 v2.3.0 // indirect
	github.com/davecgh/go-spew v1.1.2-0.20180830191138-d8f796af33cc // indirect
	github.com/golang/protobuf v1.5.4 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.4 // indirect
	github.com/pmezard/go-difflib v1.0.1-0.20181226105442-5d4384ee4fb2 // indirect
	github.com/prometheus/common v0.42.0 // indirect
	github.com/prometheus/procfs v0.10.1 // indirect
	github.com/prometheus/client_model v0.3.0  // indirect
	github.com/rogpeppe/go-internal v1.12.0 // indirect
	github.com/sirupsen/logrus v1.8.2 // indirect
	golang.org/x/sys v0.21.0 // indirect
	golang.org/x/text v0.16.0 // indirect
	golang.org/x/net v0.26.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240701130421-f6361c86f094 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)
