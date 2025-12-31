FROM alpine:latest
COPY vc-webhook-manager /vc-webhook-manager
ADD ./gen-admission-secret.sh /gen-admission-secret.sh

ENTRYPOINT ["/vc-webhook-manager"]
