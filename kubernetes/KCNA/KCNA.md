# Abbreviations

+ CNCF: Cloud Native Computing Foundation
+ CRD: Custom Resource Definition
+ OCI: Open Container Initiative
+ CNI: Container Network Interface
+ CRI: Container Runtime Interface
+ CSI: Container Storage Interface
+ SMI: Service Mesh Interface
+ RBAC: Role Based Access Control
+ CI/CD: Continuous Integration & Continuous Deployment/ Delivery
+ SRE: Site Reliability Engineer
+ SLO: Service Level Objectives
+ SLI: Service Level Indicators
+ SLA: Service Level Agreements
+ PV: Persistent Volumes
+ PVC: Persistent Volume Claims
+ SIG: Special Interest Group
+ TOC: Technical Oversight Committee
+ The 4C’s of Cloud Native security: Code, Container, Cluster, and Cloud
+ NAT: Network Address Translation
+ IaaS: Infrastructure as a Service
+ PaaS: Platform as a Service
+ FaaS: Function as a Service
+ CaaS: Containers as a Service
+ IaC: Infrastructure as Code
+ FinOps: Cloud Financial Operations



# Short Names for Resources

+ componentstatuses = cs
+ configmaps = cm
+ endpoints = ep
+ events = ev
+ limitranges = limits
+ namespaces = ns
+ nodes = no
+ persistentvolumeclaims = pvc
+ persistentvolumes = pv
+ pods = po
+ replicationcontrollers = rc
+ resourcequotas = quota
+ serviceaccounts = sa
+ services = svc
+ customresourcedefinitions = crd
+ crdsdaemonsets = ds
+ deployments = deploy
+ replicasets = rs
+ statefulsets = sts
+ horizontalpodautoscalers = hpa
+ cronjobs = cj
+ certificiaterequests = cr, crs
+ certificates = cert, certs
+ certificatesigningrequests = csr
+ ingresses = ing
+ networkpolicies = netpol
+ podsecuritypolicies = psp
+ replicasets = rs
+ scheduledscalers = ss
+ priorityclasses = pc
+ storageclasses = sc



# Kubectl Command Cheatsheet

+ References: 

  + https://kubernetes.io/docs/reference/generated/kubectl/kubectl-commands
  + https://kubernetes.io/docs/reference/kubectl/cheatsheet/

+ Commands

  + Show merged kubeconfig settings

    ```shell
    kubectl config view
    ```

  + Create objects

    + Create resource from a file or from stdin (JSON/ YAML)

      + Usage: `kubectl create -f FILENAME`

      + ```bash
        # Create a pod using the data in pod.json
        kubectl create -f ./pod.json
        # Create a pod based on the JSON passed into stdin
        cat pod.json | kubectl create -f -
        ```

    + Create Objects

      + ```bash
        # start a single instance of nginx
        kubectl create deployment nginx --image=nginx
        # create a Job which prints "Hello World"
        kubectl create job hello --image=busybox -- echo "Hello World"
        # create a CronJob that prints "Hello World" every minute
        kubectl create cronjob hello --image=busybox   --schedule="*/1 * * * *" -- echo "Hello World"
        ```

    + Apply a configuration to a resource by file name or stdin. The resource name must be specified. This resource will **be created if it doesn't exist yet**.

      + Usage: `kubectl apply (-f FILENAME | -k DIRECTORY)`

      + ```bash
        # Apply the configuration in pod.json to a pod
        kubectl apply -f ./pod.json
        # Apply resources from a directory containing kustomization.yaml - e.g. dir/kustomization.yaml
        kubectl apply -k dir/
        # Apply the JSON passed into stdin to a pod
        cat pod.json | kubectl apply -f -
        ```

    + Create and run **a particular image** in a pod

      + Usage: `kubectl run NAME --image=image [--env="key=value"] [--port=port] [--dry-run=server|client] [--overrides=inline-json] [--command] -- [COMMAND] [args...]`

      + ```bash
        # Start a nginx pod
        kubectl run nginx --image=nginx
        # Start a hazelcast pod and let the container expose port 5701
        kubectl run hazelcast --image=hazelcast/hazelcast --port=5701
        # Start a hazelcast pod and set labels "app=hazelcast" and "env=prod" in the container
        kubectl run hazelcast --image=hazelcast/hazelcast --labels="app=hazelcast,env=prod"
        ```

    + **Get document** for objects

      + Usage: `kubectl explain RESOURCE`

      + ```bash
        # Get the documentation of the resource and its fields
        kubectl explain pods
        # Get the documentation of a specific field of a resource
        kubectl explain pods.spec.containers
        ```

  + View, find resources

    + Display one or many resources

      + ```bash
        # Get commands with basic output
        kubectl get services                          # List all services in the namespace
        kubectl get pods --all-namespaces             # List all pods in all namespaces
        kubectl get pods -o wide                      # List all pods in the current namespace, with more details
        kubectl get deployment my-dep                 # List a particular deployment
        kubectl get pods                              # List all pods in the namespace
        kubectl get pod my-pod -o yaml                # Get a pod's YAML
        
        # Show labels for all pods (or any other Kubernetes object that supports labelling)
        kubectl get pods --show-labels
        
        # Get all worker nodes (use a selector to exclude results that have a label
        # named 'node-role.kubernetes.io/master')
        kubectl get node --selector='!node-role.kubernetes.io/master'
        ```

    + Show **details** of a specific resource or group of resources

      + ```bash
        # Describe commands with verbose output
        kubectl describe nodes my-node
        kubectl describe pods my-pod
        ```

  + Update resource

    + Rolling update

      + ```bash
        # Rolling update "www" containers of "frontend" deployment, updating the image
        kubectl set image deployment/frontend www=image:v2
        # Check the history of deployments including the revision 
        kubectl rollout history deployment/frontend
        # Rollback to the previous deployment
        kubectl rollout undo deployment/frontend
        # Rollback to a specific revision
        kubectl rollout undo deployment/frontend --to-revision=2
        # Watch rolling update status of "frontend" deployment until completion
        kubectl rollout status -w deployment/frontend
        # Rolling restart of the "frontend" deployment
        kubectl rollout restart deployment/frontend
        ```

    + Replace

      + ```bash
        # Replace a pod based on the JSON passed into std
        cat pod.json | kubectl replace -f -
        # Force replace, delete and then re-create the resource. Will cause a service outage.
        kubectl replace --force -f ./pod.json
        # Update a single-container pod's image version (tag) to v4
        kubectl get pod mypod -o yaml | sed 's/\(image: myimage\):.*$/\1:v4/' | kubectl replace -f -
        ```

    + Expose (Create service)

      + ```bash
        # Create a service for a replicated nginx, which serves on port 80 and connects to the containers on port 8000
        kubectl expose rc nginx --port=80 --target-port=8000
        ```

    + Add label, annotation

      + ```bash
        kubectl label pods my-pod new-label=awesome                      # Add a Label
        kubectl annotate pods my-pod icon-url=http://goo.gl/XXBTWq       # Add an annotation
        ```

    + Creates an **autoscaler** that automatically chooses and sets the number of pods that run in a Kubernetes cluster

      + ```bash
        # Auto scale a deployment "foo"
        kubectl autoscale deployment foo --min=2 --max=10
        ```

  + Patch

    + Update fields of a resource using strategic merge patch, a JSON merge patch, or a JSON patch

      + Usage: kubectl patch (-f FILENAME | TYPE NAME) [-p PATCH|--patch-file FILE]

      + ```bash
        # Partially update a node
        kubectl patch node k8s-node-1 -p '{"spec":{"unschedulable":true}}'
        # Update a container's image; spec.containers[*].name is required because it's a merge key
        kubectl patch pod valid-pod -p '{"spec":{"containers":[{"name":"kubernetes-serve-hostname","image":"new image"}]}}'
        # Update a container's image using a json patch with positional arrays
        kubectl patch pod valid-pod --type='json' -p='[{"op": "replace", "path": "/spec/containers/0/image", "value":"new image"}]'
        # Disable a deployment livenessProbe using a json patch with positional arrays
        kubectl patch deployment valid-deployment  --type json   -p='[{"op": "remove", "path": "/spec/template/spec/containers/0/livenessProbe"}]'
        # Add a new element to a positional array
        kubectl patch sa default --type='json' -p='[{"op": "add", "path": "/secrets/1", "value": {"name": "whatever" } }]'
        ```

  + Edit resources

    + Edit a resource from the default editor

      + ```bash
        # Edit the service named 'docker-registry'
        kubectl edit svc/docker-registry
        # Use an alternative editor
        KUBE_EDITOR="nano" kubectl edit svc/docker-registry
        # Edit the job 'myjob' in JSON using the v1 API format
        kubectl edit job.v1.batch/myjob -o json
        # Edit the deployment 'mydeployment' in YAML and save the modified config in its annotation
        kubectl edit deployment/mydeployment -o yaml --save-config
        ```

  + Scale resources

    + Set a new size for a deployment, replica set, replication controller, or stateful set

      + ```bash
        # Scale a replica set named 'foo' to 3
        kubectl scale --replicas=3 rs/foo
        # Scale a resource identified by type and name specified in "foo.yaml" to 3
        kubectl scale --replicas=3 -f foo.yaml
        # If the deployment named mysql's current size is 2, scale mysql to 3
        kubectl scale --current-replicas=2 --replicas=3 deployment/mysql
        # Scale multiple replication controllers
        kubectl scale --replicas=5 rc/foo rc/bar rc/baz
        ```

  + Delete resources

    + Delete resources by file names, stdin, resources and names, or by resources and label selector

      + ```bash
        # Delete a pod using the type and name specified in pod.json
        kubectl delete -f ./pod.json
        # Delete a pod with minimal delay
        kubectl delete pod unwanted --now
        # Delete pods and services with same names "baz" and "foo"
        kubectl delete pod,service baz foo
        # Delete pods and services with label name=myLabel
        kubectl delete pods,services -l name=myLabel
        # Delete all pods and services in namespace my-ns
        kubectl -n my-ns delete pod,svc --all
        # Delete all pods matching the awk pattern1 or pattern2
        kubectl get pods  -n mynamespace --no-headers=true | awk '/pattern1|pattern2/{print $1}' | xargs  kubectl delete -n mynamespace pod
        ```

  + Interact with running Pods

    + Print the logs for a container in a pod or specified resource. If the pod has only one container, the container name is optional.

      + Usage: `kubectl logs [-f] [-p] (POD | TYPE/NAME) [-c CONTAINER]`

        + -c, --container="": Print the logs of this container
        + -f, --follow: Specify if the logs should be streamed
        + -p, --previous: If true, print the logs for the **previous instance** of the container in a pod if it exists.

      + ```bash
        # dump pod logs (stdout)
        kubectl logs my-pod
        # dump pod logs, with label name=myLabel (stdout)
        kubectl logs -l name=myLabel
        # dump pod logs (stdout) for a previous instantiation of a container
        kubectl logs my-pod --previous
        # dump pod container logs (stdout, multi-container case)
        kubectl logs my-pod -c my-container
        # stream pod logs (stdout)
        kubectl logs -f my-pod
        ```

    + Display Resource (CPU/Memory) usage

      + Usage: `kubectl top`

      + ```bash
        # Show metrics for a given pod and its containers
        kubectl top pod POD_NAME --containers
        # Show metrics for a given pod and sort it by 'cpu' or 'memory'
        kubectl top pod POD_NAME --sort-by=cpu
        ```

  + Copy files and directories to and from containers

    + Usage: `kubectl cp <file-spec-src> <file-spec-dest>`

    + ```bash
      # Copy /tmp/foo_dir local directory to /tmp/bar_dir in a remote pod in the current namespace
      kubectl cp /tmp/foo_dir my-pod:/tmp/bar_dir
      # Copy /tmp/foo local file to /tmp/bar in a remote pod in a specific container
      kubectl cp /tmp/foo my-pod:/tmp/bar -c my-container
      # Copy /tmp/foo local file to /tmp/bar in a remote pod in namespace my-namespace
      kubectl cp /tmp/foo my-namespace/my-pod:/tmp/bar
      # Copy /tmp/foo from a remote pod to /tmp/bar locally
      kubectl cp my-namespace/my-pod:/tmp/foo /tmp/bar
      ```

  + Resource types

    + Print the supported API resources on the server

      + ```bash
        kubectl api-resources
        # All namespaced resources
        kubectl api-resources --namespaced=true
        # All non-namespaced resources
        kubectl api-resources --namespaced=false
        # All resources with simple output (only the resource name)
        kubectl api-resources -o name
        # All resources with expanded (aka "wide") output
        kubectl api-resources -o wide
        ```



# Tools

+ Build images: [buildah](https://buildah.io/) or [kaniko](https://github.com/GoogleContainerTools/kaniko)
+ Alternative to Docker: [podman](https://podman.io/) (provides similar API as Docker)
+ Popular public image registries: [Docker Hub](https://hub.docker.com/), [Quay](https://quay.io/)
+ Popular choices of Key-Value-Store database to store information about services: [etcd](https://github.com/coreos/etcd/), [Consul](https://www.consul.io/) or [Apache Zookeeper](https://zookeeper.apache.org/)
+ Proxy to manage network traffic: [nginx](https://www.nginx.com/), [haproxy](http://www.haproxy.org/) or [envoy](https://www.envoyproxy.io/)
+ Service mesh: [istio](https://istio.io/) and [linkerd](https://linkerd.io/)
+ Kubernetes setup:
  + Create a test cluster:
    + [Minikube](https://minikube.sigs.k8s.io/docs/), [kind](https://kind.sigs.k8s.io/), [MicroK8s](https://microk8s.io/)
  + Setup production-grade cluster on your own hardware or virtual machines:
    + [kubeadm](https://kubernetes.io/docs/reference/setup-tools/kubeadm/), [kops](https://github.com/kubernetes/kops), [kubespray](https://github.com/kubernetes-sigs/kubespray)
  + Vendors package Kubernetes into a distribution and even offer commercial support
    + [Rancher](https://rancher.com/), [k3s](https://k3s.io/), [OpenShift](https://www.redhat.com/en/technologies/cloud-computing/openshift), [VMWare Tanzu](https://tanzu.vmware.com/tanzu)
  + Consume Kubernetes from a cloud provider:
    - [Amazon (EKS)](https://aws.amazon.com/eks/), [Google (GKE)](https://cloud.google.com/kubernetes-engine), [Microsoft (AKS)](https://azure.microsoft.com/en-us/services/kubernetes-service), [DigitalOcean (DOKS)](https://www.digitalocean.com/products/kubernetes/)
+ Container runtimes: [containerd](https://containerd.io/), [CRI-O](https://cri-o.io/), Docker (will be removed in Kubernetes 1.23)
+ Security containers: [gvisor](https://github.com/google/gvisor), [Kata Containers](https://katacontainers.io/)
+ Network vendors: [Project Calico](https://www.tigera.io/project-calico/), [Weave](https://www.weave.works/oss/net/), [Cilium](https://cilium.io/)
+ DNS serve add-on in K8S for Service Discovery: [core-dns](https://kubernetes.io/docs/tasks/administer-cluster/coredns/)
+ Tools to interact with K8S: [kubernetes/dashboard](https://github.com/kubernetes/dashboard), [derailed/k9s](https://github.com/derailed/k9s), [Lens](https://k8slens.dev/), [VMware Tanzu Octant](https://github.com/vmware-tanzu/octant)
+ Package manager for Kubernetes: [Helm](https://helm.sh/)

+ Storage
  + Cloud block storage: [Amazon EBS](https://aws.amazon.com/ebs/), [Google Persistent Disks](https://cloud.google.com/persistent-disk), [Azure Disk Storage](https://azure.microsoft.com/en-us/services/storage/disks/)
  + Storage systems: [Ceph](https://ceph.io/en/), [GlusterFS](https://www.gluster.org/)
  + Traditional systems: [NFS](https://en.wikipedia.org/wiki/Network_File_System)
+ [Rook](https://rook.io/): provide cloud-native storage orchestration and integrate with battle tested storage solutions like Ceph
+ Monitor container and provide metrics to work with autoacaling: [metrics-server](https://github.com/kubernetes-sigs/metrics-server), [Prometheus Adapter for Kubernetes Metrics APIs](https://github.com/kubernetes-sigs/prometheus-adapter)
+ Autoscaler: [KEDA](https://keda.sh/) (Kubernetes-based Event Driven Autoscaler)
+ Popular CI/CD tools include: [Spinnaker](https://spinnaker.io/), [GitLab](https://gitlab.com/#), [Jenkins](https://www.jenkins.io/), [Jenkins X](https://jenkins-x.io/), [Tekton CD](https://github.com/tektoncd/pipeline), [Argo](https://argoproj.github.io/)
+ Popular GitOps frameworks that use the pull-based approach: [Flux](https://fluxcd.io/) and [ArgoCD](https://argo-cd.readthedocs.io/)
+ Ship and store logs: [fluentd](https://www.fluentd.org/) or [filebeat](https://www.elastic.co/beats/filebeat)
+ Store logs: [OpenSearch](https://opensearch.org/) or [Grafana Loki](https://grafana.com/oss/loki/)
+ [Prometheus](https://prometheus.io) (open source monitoring system) + [Grafana](https://grafana.com/grafana/) (build dashboards from collected metrics)
  + Notification tool in Prometheus when alert is firing: [Alertmanager](https://prometheus.io/docs/alerting/latest/alertmanager/)
+ Store and analyze traces: [Jaeger](https://www.jaegertracing.io/)
+ [OpenTelemetry](https://opentelemetry.io/) is a set of APIs, SDKs, tooling and integrations that are designed for the creation and management of *telemetry data* such as traces, metrics, and logs.



# Knowledge checks

### Kubernetes Fundamentals (46%)

##### 1. Kubernetes Resources

<details> <summary>Title</summary>  content!!! </details>

##### 2. Kubernetes Architecture



##### 3. Kubernetes API



##### 4. Containers



##### 5. Scheduling



### Container Orchestration (22%)

##### 1. Container Orchestration Fundamentals

##### 2. Runtime

##### 3. Security

##### 4. Networking

##### 5. Service Mesh

##### 6. Storage



### Cloud Native Architecture (16%)

<details>
<summary>Characteristics of Cloud Native Architecture</summary>

- High level of Automation
- Self-healing
- Secure by default
- Cost-efficient
- Easy-to-maintain
- Scalable

</details>

##### 1. Autoscaling

##### 2. Serverless

##### 3. Community and Governance

##### 4. Roles and Personas

##### 5. Open Standards



### Cloud Native Observability (8%)

##### 1. Telemetry & Observability

##### 2. Prometheus

##### 3. Cost Management



### Cloud Native Application Delivery (8%)

##### 1. Application Delivery Fundamentals

##### 2. GitOps

##### 3. CI/CD
