# EventHub Helm Chart

This Helm chart deploys [EventHub](https://github.com/olesku/eventhub), a high-performance publish/subscribe server over WebSocket written in modern C++.

EventHub implements the publish-subscribe pattern with topics, focusing on high performance and ease of integration. Its features include an event log for replaying messages, topic patterns for flexible subscriptions, and a simple key/value store. **A Redis-compatible backend is required for all operations**, including inter-instance communication in clustered setups. This chart provisions [Valkey](https://valkey.io/) (an open-source Redis fork) for that purpose by default.

## Prerequisites

* A running Kubernetes cluster
* Helm 3.8+ (for OCI registry support)
* (Optional, recommended for production) A default StorageClass if you enable Valkey persistence.

## Installing the Chart

The chart and its dependency are published as OCI artifacts, so no `helm repo add` is required.

A JWT secret is **required** — the chart ships no default signing key. Provide one inline or reference an existing secret (see [JWT Secret](#jwt-secret-jwtsecret)):

```bash
helm install my-eventhub oci://ghcr.io/olesku/charts/eventhub \
  --set-string jwtSecret.value="$(openssl rand -hex 32)"
```

To install from a local checkout instead (pulls the Valkey dependency first):

```bash
helm dependency update charts/eventhub
helm install my-eventhub charts/eventhub \
  --set-string jwtSecret.value="$(openssl rand -hex 32)"
```

This deploys EventHub together with a Valkey instance using the default configuration.

## Uninstalling the Chart

```bash
helm uninstall my-eventhub
```

## Parameters

### EventHub

| Parameter | Description | Default |
| --- | --- | --- |
| `replicas` | Number of EventHub deployment replicas. | `1` |
| `image.repository` | Image repository. | `ghcr.io/olesku/eventhub` |
| `image.tag` | Image tag. Empty means `v<Chart.appVersion>`. | `""` |
| `image.pullPolicy` | Image pull policy. | `IfNotPresent` |
| `http.enabled` | Enables the unsecure HTTP listener. | `true` |
| `https.enabled` | Enables the secure HTTPS listener. | `false` |
| `config` | Multiline string for the `eventhub.conf` configuration file. | (a basic config) |
| `service.type` | Kubernetes Service type. | `LoadBalancer` |
| `service.annotations` | Annotations to add to the Service. | `{}` |
| `service.http.port` | The external port for the HTTP service. | `80` |
| `service.https.port` | The external port for the HTTPS service. | `443` |
| `ingress.enabled` | If `true`, an Ingress resource will be created. | `false` |
| `ingress.annotations` | Annotations for the Ingress resource (e.g., for cert-manager or WebSocket support). | (nginx defaults) |
| `ingress.hosts` | A list of host rules for the Ingress. | (placeholder host) |
| `ingress.tls` | TLS configuration for the Ingress. | (placeholder secret) |
| `annotations` | Annotations to add to the pod metadata (e.g., for Prometheus scraping). | (prometheus scrape) |
| `resources` | CPU/memory resource requests and limits for the EventHub container. | `cpu: 500m, memory: 256Mi` |
| `affinity` | Pod affinity and anti-affinity rules. | `{}` |
| `volumes` | Additional volumes to add to the pod. | `[]` |
| `volumeMounts` | Additional volume mounts for the EventHub container. | `[]` |

> When `replicas` > 1 a `PodDisruptionBudget` (`minAvailable: 50%`) is created automatically.

### JWT Secret (`jwtSecret`)

The JWT secret is sourced into the `JWT_SECRET` environment variable.

| Parameter | Description | Default |
| --- | --- | --- |
| `jwtSecret.value` | A literal string to use as the JWT secret. If set, a Kubernetes Secret is created automatically. **Required** unless `existingSecret` is used — no default. | `""` |
| `jwtSecret.existingSecret.name` | The name of a pre-existing Secret. Takes precedence over `jwtSecret.value`. | unset |
| `jwtSecret.existingSecret.key` | The key within the existing secret that holds the token. | `jwt_secret` |

### Valkey dependency (`valkey`)

This chart includes the [Valkey chart](https://github.com/valkey-io/valkey-helm/) as a dependency, enabled by default. To use an external Redis/Valkey instead, set `valkey.enabled: false` and point EventHub at your own host.

| Parameter | Description | Default |
| --- | --- | --- |
| `valkey.enabled` | Deploy the bundled Valkey dependency. | `true` |
| `valkey.image.repository` | Valkey image repository. | `docker.io/valkey/valkey` |
| `valkey.image.tag` | Valkey image tag. | `8.1.4` |
| `valkey.auth.enabled` | Enable password authentication on Valkey. | `false` |
| `valkey.dataStorage.enabled` | Enable persistence for Valkey. Recommended for production. | `false` |
| `valkey.resources` | CPU/memory resource requests for Valkey. | `cpu: 100m, memory: 256Mi` |

> The legacy `redis.*` values are **not** supported. The chart fails fast with a clear message if `redis.enabled` is set — use `valkey.*` instead.

## Configuration example

```yaml
# values.yaml — clustered deployment with an existing JWT secret and persistent Valkey
replicas: 3

config: |-
  log_level                = info
  enable_cache             = false
  ping_interval            = 30
  handshake_timeout        = 10

jwtSecret:
  existingSecret:
    name: "my-external-jwt"
    key: "jwt_secret"

valkey:
  enabled: true
  dataStorage:
    enabled: true
```

## Upgrading

```sh
helm upgrade my-eventhub oci://ghcr.io/olesku/charts/eventhub
```

Pin a specific chart version with `--version X.Y.Z` (no leading `v` — the chart version is the bare SemVer). The chart version tracks the eventhub application release.
