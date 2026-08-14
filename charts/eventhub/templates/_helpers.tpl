{{- define "eventhub.fullname" -}}
{{- printf "%s-%s" .Release.Name .Chart.Name | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "eventhub.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "eventhub.labels" -}}
app.kubernetes.io/name: {{ include "eventhub.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
helm.sh/chart: {{ include "eventhub.chart" . }}
{{- end -}}

{{- define "eventhub.selectorLabels" -}}
app.kubernetes.io/name: {{ include "eventhub.fullname" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end -}}

{{- define "eventhub.identifier" -}}
{{- $chartname := .Chart.Name | replace "-" "_" -}}
{{- $releasename := .Release.Name | replace "-" "_" -}}
{{- $namespace := .Release.Namespace | replace "-" "_" -}}
{{- printf "%s_%s_%s" $chartname $releasename $namespace -}}
{{- end -}}

{{- define "eventhub.valkey.host" -}}
{{- printf "%s-valkey" .Release.Name -}}
{{- end -}}

{{- define "eventhub.jwtSecretName" -}}
{{- printf "%s-jwt-secret" (include "eventhub.fullname" .) -}}
{{- end -}}

{{- define "eventhub.tlsSecretName" -}}
{{- $tls := .Values.https.tls | default dict -}}
{{- if $tls.existingSecret -}}
{{- $tls.existingSecret -}}
{{- else -}}
{{- printf "%s-tls" (include "eventhub.fullname" .) -}}
{{- end -}}
{{- end -}}
