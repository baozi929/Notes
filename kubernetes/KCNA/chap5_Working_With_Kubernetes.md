# Introduction

1. Chapter Overview
   + Kubernetes objects, their purpose and how to interact with them
     + smallest compute unit: Pod
   + tasks that developers or administrators can perform:
     + deploy workload
     + configuration management
     + cross-node networking
     + routing of external traffic
     + load balancing
     + scaling of the pods
2. Learning Objectives
   + Explain what a **Kubernetes object** is and how to describe it. 
   + Discuss the **Pod concept** and the problems it solves. 
   + Understand how to **scale and schedule Pods with workload resources**. 
   + Understand how to **abstract Pods with services** and how to **expose** them.



# Working with Kubernetes

1. Kubernetes Objects

   + Reference:

     + https://kubernetes.io/docs/concepts/overview/working-with-objects/kubernetes-objects/

   + Kubernetes provide a lot of **abstract resources** (objects)

     + can be used to describe how the workload should be handled

   + Category

     + workload-oriented
       + handle container workloads
     + infrastructure-oriented
       + handle configuration, networking, security, ...

   + How to use the objects:

     + describe the objects with YMAL and send them to api-server

       + example:

         + ```
           apiVersion: apps/v1
           kind: Deployment
           metadata:
             name: nginx-deployment
           spec: 
             selector:
               matchLabels:
                 app: nginx
             replicas: 2 # tells deployment to run 2 pods matching the template
             template:
               metadata:
                 labels:
                   app: nginx
               spec:
                 containers:
                 - name: nginx
                   image: nginx:1.19
                   ports:
                   - containerPort: 80
           ```

         + `apiVersion`:

           + Which **version of the Kubernetes API** you're using to create this object

         + `kind`:

           + What **kind of object** you want to create
           + For example: Deployment, Pod, ...

         + `matadata`:

           + Data that helps **uniquely identify the object**, including a `name` string, `UID`, and optional `namespace`

         + `spec`:

           + What **state** you desire for the object

2. Interacting with Kubernetes

   + Access the API with the official command line interface client: `kubectl`

     + install kubectl: https://kubernetes.io/docs/tasks/tools/#kubectl

   + Commands:

     + list available objects in cluster

       + ```
         $ kubectl api-resources
         
         NAME                    SHORTNAMES  APIVERSION  NAMESPACED  KIND
         ...
         configmaps              cm          v1          true        ConfigMap
         ...
         namespaces              ns          v1          false       Namespace
         nodes                   no          v1          false       Node
         persistentvolumeclaims  pvc         v1          true        PersistentVolumeClaim
         ...
         pods                    po          v1          true        Pod
         ...
         services                svc         v1          true        Service
         ```

       + **short names**

     + learn more about pods

       + ```
         $ kubectl explain pod
         
         KIND:     Pod
         VERSION:  v1
         
         DESCRIPTION:
              Pod is a collection of containers that can run on a host. This resource is     
              created by clients and scheduled onto hosts. 
         
         FIELDS: 
            apiVersion <string>     
              APIVersion defines the versioned schema of this representation of an
              object. Servers should convert recognized schemas to the latest internal         
              value, and may reject unrecognized values.
         ...
            kind <string>
         ...
            metadata <Object>
         ...
            spec <Object>
         ```

     + learn more about pod spec (drill down in the object definition with format: `<type>.<fieldName>[.<fieldName>]`)

       + ```
         $ kubectl explain pod.spec
         
         KIND:     Pod
         VERSION:  v1 
         
         RESOURCE: spec <Object>  
         
         DESCRIPTION:
              Specification of the desired behavior of the pod. More info:
         
         https://git.k8s.io/community/contributors/devel/sig-architecture/api-conventions.md#spec-and-status      
         
              PodSpec is a description of a pod. 
         
         FIELDS:
            activeDeadlineSeconds <integer>     
              Optional duration in seconds the pod may be active on the node relative to       
              StartTime before the system will actively try to mark it failed and kill         
              associated containers. Value must be a positive integer. 
         
            affinity <object>     
              If specified, the pod's scheduling constraints 
         
            automountServiceAccountToken <boolean>     
              AutomountServiceAccountToken indicates whether a service account token           
              should be automatically mounted. 
         
            containers <[]Object> -required-
         ...
         ```

     + create an object in Kubernetes from a YAML file

       + ```
         kubectl create -f <your-file>.yaml
         ```

   + Tools to interact with Kubernetes:

     + [kubernetes/dashboard](https://github.com/kubernetes/dashboard)
     + [derailed/k9s](https://github.com/derailed/k9s)
     + [Lens](https://k8slens.dev/)
     + [VMware Tanzu Octant](https://github.com/vmware-tanzu/octant)

   + Most popular package manager for Kubernetes

     + [Helm](https://helm.sh/) allows easier updates and interaction with objects
       + Helm packages Kubernetes objects in so-called Charts (can be shared with others via a registry)

3. Demo: kubectl

   + [Overview of kubectl](https://kubernetes.io/docs/reference/kubectl/overview/)

     + Can be used to interact with the cluster
     + kubectl uses [kubeconfig files](https://kubernetes.io/docs/concepts/configuration/organize-cluster-access-kubeconfig/)
       + the link here will give instructions on how to manage multiple clusters and multiple users inside a kubuconfig file

   + Commands

     + check kubectl config

       + ```
         kubectl config view
         ```

       + file consists of 3 different sections:

         + clusters
         + context (combine clusters and users)
         + users

     + use `kubectl --help` to check other commands

   + Create Kubernetes object

     1. Describe the object with YMAL file

        + Example: pod.yaml

          + ```
            apiVersion: v1
            kind: Pod
            metadata:
              name: nginx
            spec:
              containers:
              - name: nginx
                image: nginx:1.20
                ports:
                - containerPort: 80
            ```

          + A pod running a nginx container that opens port 80

     2. Create object with YAML file

        + ```
          kubectl create -f pod.yaml
          ```

     3. Get running pods

        + ```
          kubectl get pod
          ```

     4. Delete pod

        + ```
          kubectl delete pod nginx
          ```

   + Other useful commands:

     + explain command, help you learn how to write ye YAML file

       + ```
         kubectl explain --help
         ```

4. Pod Concept

   + Pod

     + Pod is a group of **one or more containers** that **share an isolation layer** of namespaces and cgroups
       + **all containers inside a pod share an IP address and can share via the filesystem**
     + smallest deployable unit of computing in Kubernetes
       +  -> **Kubernetes is not interacting (creating and managing) with containers directly**

   + Why Pod?

     + pod concept was introduced to allow **running a combination of multiple processes that are interdependent**

   + Multiple containers share namespace to form a pod

     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap5_MultipleContainersShareNamespacesToFormAPod.png)

       + Example: Pod object with 2 containers:

         + ```
           apiVersion: v1
           kind: Pod
           metadata:
             name: nginx-with-sidecar
           spec:
             containers:
             - name: nginx
               image: nginx:1.19
               ports:
               - containerPort: 80
             - name: count
               image: busybox:1.34
               args: [/bin/sh, -c,
                       'i=0; while true; do echo "$i: $(date)"; i=$((i+1)); sleep 1; done']
           ```

       + cannot scale the containers individually

     + **Sidecar container**

       + Using a second container that supports the main application, called a sidecar container

     + initContainers

       + used to **start containers before the main applications starts**

       + Example: 

         + ```
           apiVersion: v1
           kind: Pod
           metadata:
             name: myapp-pod
             labels:
               app: myapp
           spec:
             containers:
             - name: myapp-container
               image: busybox
               command: ['sh', '-c', 'echo The app is running! && sleep 3600']
             initContainers:
             - name: init-myservice
               image: busybox
               command: ['sh', '-c', 'until nslookup myservice; do echo waiting for myservice; sleep 2; done;']
           ```

         + init container `init-myservice` tries to reach another service -> main container will start after it completes

   + Important settings can be set for every container in a Pod

     + `resources`: 
       + set a **resource request and a maximum limit** for CPU and Memory
       + https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
     + `livenessProbe`:
       + **health check** that periodically checks if your application is still alive (Containers can be restarted if the check fails)
       + https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/
     + `securityContext`:
       + set **user & group settings**, as well as **kernel capabilities**
       + https://kubernetes.io/docs/tasks/configure-pod-container/security-context/

5. Demo: Pods

   + https://kubernetes.io/docs/concepts/workloads/pods/

   + Difference between Pod & Container

     + Container
       + create: `docker run`
       + check running containers: `docker ps`
     + Pod
       + create pod: `kubectl run nginx --image=nginx:1.19`
       + check running pods: `kubectl get pods`
       + get detailed info about pod: `kubectl describe pod nginx`
         + Name, IP address, ...
           + example: use IP to talk to the nginx container, `curl http://192.168.36.137`
         + Events: like image download, create container, assign Pod to worker node

   + Example to start Pod with 2 containers:

     + pod-two-containers.yaml

       + ```
         apiVersion: v1
         kind: Pod
         metadata:
           name: nginx-with-sidecar
         spec:
           containers:
           - name: nginx
             image: nginx:1.19
             ports:
             - containerPort: 80
           - name: count
             image: busybox:1.34
             args: [/bin/sh, -c,
                     'i=0; while true; do echo "$i: $(date)"; i=$((i+1)); sleep 1; done']
         ```

     + ```
       $ kubectl create -f pod-two-containers.yaml
       pod/nginx-with-sidecar created
       ```

     + ```
       $ kubectl get pods
       NAME				READY	STATUS		RESTARTS	AGE
       nginx-with-sidecar  1/1		Running 	0			5s
       ```

     + check detailed info: `kubectl describe pod nginx-with-sidecar`

       + check information and Events for 2 containers

6. Workload Objects

   + Use **controller objects** to manage pods

     + if a pod is lost because a node failed -> it's gone forever
     + to make sure **a defined number of Pod copies runs all the time** -> need controller objects

   + Kubernetes Objects

     + **ReplicaSet**:
       + usage:
         + ensures a desired number of pod is running at any given time
         + can be used to **scale out applications** and improve their availability
           + How: start multiple copies of a pod definition
     + **Deployment**:
       + usage:
         + describe the complete application lifecycle
           + How: manage multiple ReplicaSets that get updated when the application is changed by providing a new container image
       + Deployments are perfect to run **stateless applications** in Kubernetes
     + **StatefulSet**:
       + usage:
         + run **stateful applications** like databases
       + StatefulSet VS pods and containers
         + statefulset:
           + try to retain IP addresses of pods and give them a stable name
           + persistent storage
           + more graceful handling of scaling and updates
         + pods and containers: ephemeral
     + **DaemonSet**:
       + usage: 
         + ensures that **a copy of a Pod runs on all (or some) nodes of your cluster**
       + DaemonSets are perfect to run **infrastructure-related workload**
         + for example monitoring or logging tools.
     + **Job**:
       + usage:
         + creates one or more Pods that execute a task and terminate afterwards
       + Job objects are perfect to run **one-shot scripts**
         + like database migrations or administrative tasks
     + **CronJob**:
       + VS Job
         + CronJobs add a time-based configuration to jobs
           + -> can run Jobs periodically, such as doing a backup job every night at 4am

   + Interactive Tutorial

     + Deploy app: https://kubernetes.io/docs/tutorials/kubernetes-basics/deploy-app/deploy-intro/

       + ```
         $ kubectl create deployment kubernetes-bootcamp --image=gcr.io/google-samples/kubernetes-bootcamp:v1
         deployment.apps/kubernetes-bootcamp created
         ```

       + ```
         $ kubectl get deployments
         NAME                  READY   UP-TO-DATE   AVAILABLE   AGE
         kubernetes-bootcamp   1/1     1            1           49s
         ```

     + Explore app: https://kubernetes.io/docs/tutorials/kubernetes-basics/explore/explore-intro/

       + ```
         kubectl logs $POD_NAME
         kubectl exec $POD_NAME -- env
         kubectl exec -ti $POD_NAME -- bash
         ```

7. Demo: Workload Objects

   + Simple case:

     + pod.yaml

       + ```
         apiVersion: v1
         kind: Pod
         metadata:
           name: simple-nginx-pod
         spec:
           containers:
           - name: nginx
             image: nginx:1.19
             ports:
             - containerPort: 80
         ```

     + create pod

       + ```
         kubectl create -f pod.yaml
         ```

       + (can use KubeView to visualize details)

   + Create ReplicaSet object

     + replicaset.yaml

       + ```
         apiVersion: v1
         kind: ReplicaSet
         metadata:
           name: nginx
         spec:
           replicas: 3
           selector:
             matchLabels:
               app: nginx
           template:
           	metadata:
           	  label:
           	    app: nginx
           	spec:
               containers:
               - name: nginx
                 image: nginx:1.19
                 ports:
                 - containerPort: 80
         ```

     + create replicaset

       + ```
         kubectl create -f replicaset.yaml
         ```

     + get pods

       + ```
         $ kubectl get pods
         NAME			READY	STATUS		RESTARTS	AGE
         nginx-8l6sp		1/1		Running 	0			5s
         nginx-9bnkz		1/1		Running 	0			5s
         nginx-bvcfz		1/1		Running 	0			5s
         ```

       + ```
         $ kubectl get pods -o wide
         NAME			READY	STATUS		RESTARTS	AGE	IP				NODE	........
         nginx-8l6sp		1/1		Running 	0			5s	192.168.36.154	lfs250-worker
         nginx-9bnkz		1/1		Running 	0			5s	192.168.36.156	lfs250-worker
         nginx-bvcfz		1/1		Running 	0			5s	192.168.36.155	lfs250-worker
         ```

     + scale up/ down number of copies (use `kubectl scale --help` for help)

       + ```
         kubectl scale --replicas=10 rs/nginx
         ```

   + Create deployment object

     + Deployment object can
     
       + manage multiple ReplicaSets
       + control how a rolling upgrade of pods should be done in Kubernetes
     
     + deployment.yaml
     
       + ```
         apiVersion: apps/v1
         kind: Deployment
         metadata:
           labels:
             app: nginx
           name: nginx
         spec:
           replicas: 3
           selector:
             matchLabels:
               app: nginx
           template:
           	metadata:
           	  creationTimestamp: null
           	  label:
           	    app:nginx
           	spec:
               containers:
               - name: nginx
                 image: nginx:1.19
         ```
     
     + create deployment
     
       + ```
         kubectl create -f deployment.yaml
         ```
     
     + scale deployment
     
       + ```
         kubectl scale --replicas=10 deployment/nginx
         ```
     
     + update deployment
     
       + ```
         kubectl set image deployment/nginx nginx=nginx:1.20
         ```
     
       + Kubernetes will start a new ReplicaSet will be started, and scale down the old ReplicaSet
     
       + Wait for seconds, we'll get a new ReplicaSet with new image

8. Networking Objects

   + Use  **Service** and **Ingress objects** to **define and abstract networking**

   + **Service**:

     + Services can be used to **expose a set of pods as a network service**
     + Service Types:
       + ClusterIP
         + The most common service type
           + can be used as **round-robin load balancer**
         + A ClusterIP is a **virtual IP inside Kubernetes** that can be used as a single endpoint for a set of pods
         + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap5_ClusterIP.png)
       + NodePort
         + extends the ClusterIP by **adding simple routing rules**
           + **allows routing external traffic to the cluster**
         + How: opens a port (default between 30000-32767) on every node in the cluster and maps it to the ClusterIP
       + LoadBalancer
         + extends the NodePort by deploying an external LoadBalancer instance
       + ExternalName
         + special service type that has no routing
         + using the Kubernetes internal DNS server to create a DNS alias
           + for example: create a simple alias to resolve a rather complicated hostname
         + useful when you want to reach external resources from your Kubernetes cluster
       + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap5_ClusterIPNodePortandLoadBalancer.png)

   + **Ingress objects**:

     + compared with service: more flexible to expose applications
     + Ingress provides a means to **expose HTTP and HTTPS routes from outside of the cluster for a service within the cluster**
     + How it works: configuring routing rules that a user can set and implement with an **ingress controller**
     + Example:
       + where an Ingress sends all its traffic to one Service
         + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap5_Ingress.png)
         + https://kubernetes.io/docs/concepts/services-networking/ingress/
     + Features of ingress controllers:
       + LoadBalancing
       + TLS offloading/termination
       + Name-based virtual hosting
       + Path-based routing
       + ...

   + NetworkPolicy

     + a simple **IP firewall** (OSI Layer 3 or 4) that can control traffic based on rules
     + can define rules for incoming (**ingress**) and outgoing traffic (**egress**)
     + typical use case:
       + **restricting the traffic between two different namespaces**

   + Interact tutorial:

     + Using a Service to Expose Your App

       + https://kubernetes.io/docs/tutorials/kubernetes-basics/expose/expose-intro/

     + Steps:

       + List Pods:

         + ```
           $ kubectl get pods
           NAME                                  READY   STATUS    RESTARTS   AGE
           kubernetes-bootcamp-fb5c67579-tj5kr   1/1     Running   0          2m55s
           ```

       + List Services:

         + ```
           $ kubectl get services
           NAME         TYPE        CLUSTER-IP   EXTERNAL-IP   PORT(S)   AGE
           kubernetes   ClusterIP   10.96.0.1    <none>        443/TCP   4m6s
           ```

       + Create a new service and expose it to external traffic

         + ```
           kubectl expose deployment/kubernetes-bootcamp --type="NodePort" --port 8080
           ```

         + if we get service now, Pod kubernetes-bootcamp is also exposed

           + ```
             $ kubectl get services
             NAME                  TYPE        CLUSTER-IP      EXTERNAL-IP   PORT(S)          AGE
             kubernetes            ClusterIP   10.96.0.1       <none>        443/TCP          6m40s
             kubernetes-bootcamp   NodePort    10.97.223.117   <none>        8080:32284/TCP   2m1s
             ```

       + Find out what port was opened externally

         + ```
           $ kubectl describe services/kubernetes-bootcamp
           Name:                     kubernetes-bootcamp
           Namespace:                default
           Labels:                   app=kubernetes-bootcamp
           Annotations:              <none>
           Selector:                 app=kubernetes-bootcamp
           Type:                     NodePort
           IP Families:              <none>
           IP:                       10.97.223.117
           IPs:                      10.97.223.117
           Port:                     <unset>  8080/TCP
           TargetPort:               8080/TCP
           NodePort:                 <unset>  32284/TCP
           Endpoints:                172.18.0.3:8080
           Session Affinity:         None
           External Traffic Policy:  Cluster
           Events:                   <none>
           ```

       + Connect to the pod 

         + ```
           curl $(minikube ip):$NODE_PORT
           ```

9. Demo: Using Services

   + deployment2.yaml

     + ```
       apiVersion: apps/v1
       kind: Deployment
       metadata:
         labels:
           app: echoserver
         name: echoserver
       spec:
         replicas: 3
         selector:
           matchLabels:
             app: echoserver
         template:
         	metadata:
         	  creationTimestamp: null
         	  label:
         	    app: echoserver
         	spec:
             containers:
             - image: k8s.gcr.io/echoserver:1.10
               name: echoserver
               ports:
               - containerPort: 8080
       ```

     + echoserver in this case will give us the name of the pod if we call it via HTTP

   + Create deployment

     + ```
       kubectl create -f deployment2.yaml
       ```

   + Get information for 3 Pods

     + ```
       kubectl get pods -o wide
       ```

   + Access echoserver (all 3 pods)

     + ```
       curl IP:8080
       ```

     + IP here is IP for each pods (IP1, IP2, IP3)

   + We want to **distribute traffic across the 3 IP addresses**

     + ```
       kubectl expose --help
       ```

     + ```
       kubectl expose deployment echoserver --port=8080
       ```

   + Check service information

     + ```
       $ kubectl get svc
       NAME		TYPE		CLUSTER-IP		EXTERNAL-IP	PORT(S) 	AGE
       echoserver	ClusterIP	10.110.116.158	<none>		8080/TCP	53s
       kubernetes	ClusterIP	10.96.0.1		<none>		443/TCP		144m
       ```

     + ClusterIP is a virtual IP, we can send a request to that IP address and get answer from one of the 3 Pods

       + ```
         curl 10.110.116.158:8080
         ```

       + response will come from 1 of the 3 echoserver Pods

   + Scale up to 6 replicas

     + ```
       kubectl scale deployment echoserver --replicas=6
       ```

     + ```
       watch curl 10.110.116.158:8080
       ```

       + get an answer from a different pod every 2 seconds

10. Volume & Storage Objects

    + Requirements:

      + persistent storage, especially when that storage spans across multiple nodes

    + Solutions

      + Kubernetes: **volumes** as a part of Pod

      + Example:

        + **hostPath** volume mount:

          + ```
            apiVersion: v1
            kind: Pod
            metadata:
              name: test-pd
            spec:
              containers:
              - image: k8s.gcr.io/test-webserver
                name: test-container
                volumeMounts:
                - mountPath: /test-pd
                  name: test-volume
              volumes:
              - name: test-volume
                hostPath:
                  # directory location on host
                  path: /data
                  # this field is optional
                  type: Directory
            ```

          + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap5_hostPathvolumemount.png)

    + Usage

      1. **sharing data** between multiple containers within the same Pod
         + allows for great flexibility for sidecar pattern
      2. **preventing data loss** when a Pod crashes and is restarted on the same node
         + all data is lost unless written to a volume

    + Commonly used storage:

      + Cloud block storage:  [Amazon EBS](https://aws.amazon.com/ebs/), [Google Persistent Disks](https://cloud.google.com/persistent-disk), [Azure Disk Storage](https://azure.microsoft.com/en-us/services/storage/disks/)
      + Storage systems: [Ceph](https://ceph.io/en/), [GlusterFS](https://www.gluster.org/)
      + Traditional systems: [NFS](https://en.wikipedia.org/wiki/Network_File_System)

    + Container Storage Interface (CSI)

      + Why we need CSI?

        + Only a few examples of storage that can be used in Kubernetes -> To make the user experience more uniform, CSI is introduced.

      + What does CSI do?

        + CSI allows the storage vendor to write a plugin (storage driver) that can be used in Kubernetes

      + Objects to use CSI

        + **PersistentVolumes (PV)**

          + An abstract description for **a slice of storage**
          + Object configuration includes:
            + type of volumn
            + volumn size
            + access mode
            + unique identifiers and information how to mount it

        + **PersistentVolumeClaims (PVC)**

          + A request for storage by a user
          + If the cluster has multiple persistent volumes, the user can create a PVC which will **reserve a persistent volume according to the user's needs**

        + Example:

          + PV, PVC and Pod

            + A PersistentVolume that uses an AWS EBS volume implemented with a CSI driver
            + After the PersistentVolume is provisioned, a developer can reserve it with a PersistentVolumeClaim
            + The last step is using the PVC as a volume in a Pod, just like the hostPath example we saw before.

          + ```
            apiVersion: v1
            kind: PersistentVolume
            metadata:
              name: test-pv
            spec:
              capacity:
                storage: 50Gi
              volumeMode: Filesystem
              accessModes:
                - ReadWriteOnce
              csi:
                driver: ebs.csi.aws.com
                volumeHandle: vol-05786ec9ec9526b67
            ---
            apiVersion: v1
            kind: PersistentVolumeClaim
            metadata:
              name: ebs-claim
            spec:
              accessModes:
                - ReadWriteOnce
              resources:
                requests:
                  storage: 50Gi
            ---
            apiVersion: v1
            kind: Pod
            metadata:
              name: app
            spec:
              containers:
                - name: app
                  image: centos
                  command: ["/bin/sh"]
                  args:
                    ["-c", "while true; do echo $(date -u) >> /data/out.txt; sleep 5; done"]
                  volumeMounts:
                    - name: persistent-storage
                      mountPath: /data
              volumes:
                - name: persistent-storage
                  persistentVolumeClaim:
                    claimName: ebs-claim
            ```

    + Rook

      + [Rook](https://rook.io/) provide **cloud-native storage orchestration** and integrate with battle tested storage solutions like Ceph

11. Configuration Objects

    + Recommended in the Twelve-Factor App

      + **store configuration in environment** (https://12factor.net/config)

    + Requirements for running an application:

      + application code
      + some libraries
      + ......
        + config files
          + configuration like connection strings is need when **connect to other services, databases, storage system or caches**

    + Bad practice:

      + incorporate the configuration directly into the container build
        + Because change of configuration requires to rebuild entire image and redeploy the entire pod

    + How the problem solved in Kubernetes?

      + **decouple the configuration from the Pods** with a **ConfigMap**

        + Usage of ConfigMaps:

          + store whole configuration files or variables as **key-value pairs**

        + How to use a ConfigMap?

          1. Mount a ConfigMap as **a volume in Pod**

          2. Map variables from a ConfigMap to **environment variables of a Pod**

        + Example of a ConfigMap

          + ```
            apiVersion: v1
            kind: ConfigMap
            metadata:
              name: nginx-conf
            data:
              nginx.conf: |
                user nginx;
                worker_processes 3;
                error_log /var/log/nginx/error.log;
            ...
                  server {
                      listen     80;
                      server_name _;
                      location / {
                          root   html;
                          index  index.html index.htm; } } }
            ```

        + Use ConfigMap to create a Pod

          + ```
            apiVersion: v1
            kind: Pod
            metadata:
              name: nginx
            spec:
              containers:
              - name: nginx
                image: nginx:1.19
                ports:
                - containerPort: 80
                volumeMounts:
                - mountPath: /etc/nginx
                  name: nginx-conf
              volumes:
              - name: nginx-conf
                configMap:
                  name: nginx-conf
            ```

    + Object to store sensitive infomation:

      + **Secrets**
        + Secrets are very much related to ConfigMaps and basically their only difference is that **secrets are base64 encoded**
        + **Not secure to use secret**
          + -> In cloud-native environments purpose-built **secret management tools** have emerged that integrate very well with Kubernetes
            + [HashiCorp Vault](https://www.vaultproject.io/)

12. Autoscaling Objects

    + 3 Autoscaling mechanisms to **scale workload** in a Kubernetes cluster
    
      + Horizontal Pod Autoscaler (HPA), Most used autoscaler in Kubernetes: 
        + Reference: https://kubernetes.io/docs/tasks/run-application/horizontal-pod-autoscale/
        + The HPA can watch Deployments or ReplicaSets and **increase the number of Replicas if a certain threshold is reached**
          + Example:
            + Pod can use 500Mib & Configure a threshold of 80% -> if usage is over 400MiB (80%), a second Pod will get scheduled
      + Cluster Autoscaler
        + Reference: https://github.com/kubernetes/autoscaler/tree/master/cluster-autoscaler
        + Cluster Autoscaler can **add new worker nodes to the cluster** if the demand increases
          + -> can work together with HPA
      + Vertical Pod Autoscaler
        + Reference: https://github.com/kubernetes/autoscaler/tree/master/vertical-pod-autoscaler
        + allows **Pods to increase the resource requests and limits** dynamically
        + vertical scaling is limited by the node capacity
    
    + Autoscaling in Kubernetes is NOT available out of the box
    
      + Require installing add-on like [metrics-server](https://github.com/kubernetes-sigs/metrics-server).
    
    + Interactive Tutorial: Scale Your App
    
      + https://kubernetes.io/docs/tutorials/kubernetes-basics/scale/scale-intro/
    
      + get deployments
    
        + ```
          $ kubectl get deployments
          NAME                  READY   UP-TO-DATE   AVAILABLE   AGE
          kubernetes-bootcamp   1/1     1            1           61s
          ```
    
        + *NAME* lists the names of the Deployments in the cluster.
    
        + *READY* shows the ratio of CURRENT/DESIRED replicas
    
        + *UP-TO-DATE* displays the **number of replicas that have been updated to achieve the desired state**
    
        + *AVAILABLE* displays how many replicas of the application are available to your users.
    
        + *AGE* displays the amount of time that the application has been running.
    
      + get replicaset infomation
    
        + ```
          $ kubectl get rs
          NAME                            DESIRED   CURRENT   READY   AGE
          kubernetes-bootcamp-fb5c67579   1         1         1       117s
          ```
    
        + *DESIRED* displays the **desired number of replicas of the application**, which you define when you create the Deployment. This is the desired state.
    
        + *CURRENT* displays how many replicas are currently running.
    
      + scale up
    
        + ```
          $ kubectl scale deployments/kubernetes-bootcamp --replicas=4
          deployment.apps/kubernetes-bootcamp scaled
          ```
    
        + ```
          $ kubectl get deployments
          NAME                  READY   UP-TO-DATE   AVAILABLE   AGE
          kubernetes-bootcamp   4/4     4            4           8m47s
          ```
    
        + ```
          $ kubectl get pods -o wide
          NAME                                  READY   STATUS    RESTARTS   AGE     IP           NODE       NOMINATED NODE   READINESS GATES
          kubernetes-bootcamp-fb5c67579-2xpzt   1/1     Running   0          8m51s   172.18.0.2   minikube   <none>           <none>
          kubernetes-bootcamp-fb5c67579-fvnlk   1/1     Running   0          2m4s    172.18.0.9   minikube   <none>           <none>
          kubernetes-bootcamp-fb5c67579-pc6qs   1/1     Running   0          2m4s    172.18.0.7   minikube   <none>           <none>
          kubernetes-bootcamp-fb5c67579-znf5t   1/1     Running   0          2m4s    172.18.0.8   minikube   <none>           <none>
          ```
    
        + ```
          $ kubectl describe deployments/kubernetes-bootcamp
          Name:                   kubernetes-bootcamp
          Namespace:              default
          CreationTimestamp:      Tue, 01 Mar 2022 14:45:54 +0000
          Labels:                 app=kubernetes-bootcamp
          Annotations:            deployment.kubernetes.io/revision: 1
          Selector:               app=kubernetes-bootcamp
          Replicas:               4 desired | 4 updated | 4 total | 4 available | 0 unavailable
          StrategyType:           RollingUpdate
          MinReadySeconds:        0
          RollingUpdateStrategy:  25% max unavailable, 25% max surge
          Pod Template:
            Labels:  app=kubernetes-bootcamp
            Containers:
             kubernetes-bootcamp:
              Image:        gcr.io/google-samples/kubernetes-bootcamp:v1
              Port:         8080/TCP
              Host Port:    0/TCP
              Environment:  <none>
              Mounts:       <none>
            Volumes:        <none>
          Conditions:
            Type           Status  Reason
            ----           ------  ------
            Progressing    True    NewReplicaSetAvailable
            Available      True    MinimumReplicasAvailable
          OldReplicaSets:  <none>
          NewReplicaSet:   kubernetes-bootcamp-fb5c67579 (4/4 replicas created)
          Events:
            Type    Reason             Age    From                   Message
            ----    ------             ----   ----                   -------
            Normal  ScalingReplicaSet  9m54s  deployment-controller  Scaled up replica set kubernetes-bootcamp-fb5c67579 to 1
            Normal  ScalingReplicaSet  3m5s   deployment-controller  Scaled up replica set kubernetes-bootcamp-fb5c67579 to 4
          ```