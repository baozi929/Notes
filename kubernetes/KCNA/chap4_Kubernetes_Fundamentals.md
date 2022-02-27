# Introduction

1. Chapter Overview
   + Kubernetes
     + Open-source **container orchestration** platform
     + Can be used to **automate deployment, scaling and the management of containerized workloads**
2. Learning Objectives
   + The **basic architecture** of Kubernetes
   + The different components of **control plane** and **worker nodes**
   + How to get started with the **Kubernetes setup**
   + How Kubernetes is **running containers**
   + The **scheduling** concepts of Kubernetes.



# Kubernetes Fundamentals

1. Kubernetes Architecture

   + Kubernetes clusters (High-level perspective)
     + **Control plane node(s)**
       + manage the cluster and control various tasks
         + example: deployment, scheduling, self-healing of containerized workloads, ...
     + **Worker nodes**
       + run applications
       + behavior is controlled by the control plane node
         + example: start a container
   + Kubernetes architecture
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap4_KubernetesArchitecture.png)
     + Components of control panel nodes
       + kube-apiserver
         + all other components interact with the api-server
         + user access the cluster with the api-server
       + etcd
         + database which holds the state of the cluster
         + https://etcd.io/
       + kube-scheduler
         + assigns Pods to Nodes (based on different properties like CPU and memory)
       + kube-controller-manager
         + contains different **non-terminating control loops** that **manage the state of the cluster**
       + cloud-controller-manager (optional)
         + interact with the API of cloud providers to create **external resources** (like load balancer, storage or security groups)
     + Components of worker nodes
       + container runtime
         + responsible for running the containers on the worker node
         + popular choice:
           + containerd: https://containerd.io/
       + kubelet
         + agent that **runs on every worker node** in the cluster
         + talks to the api-server and the container runtime to **handle the final stage of starting containers**
       + kube-proxy
         + **handles inside and outside communication of your cluster**
         + rely on the networking capabilities of the underlying operating system if possible
   + Key points:
     + Applications already started on a worker node will continue to run even when the control plane is not available
     + Kubernetes Namespace
       + don't confuse with kernel namespaces (used to isolate containers)
       + Kubernetes namespace can be used to:
         + divide a cluster to multiple virtual clusters (used for multi-tenancy when multiple teams share a cluster)

2. Kubernetes Setup

   + Tools for test "cluster"

     + Minikube: https://minikube.sigs.k8s.io/docs/
     + kind: https://kind.sigs.k8s.io/
     + MicroK8s: https://microk8s.io/

   + Set up production-grade cluster

     + kubeadm: https://kubernetes.io/docs/reference/setup-tools/kubeadm/
     + kops: https://github.com/kubernetes/kops
     + kubespray: https://github.com/kubernetes-sigs/kubespray

   + Packaging Kubernetes into a distribution

     + Rancher: https://rancher.com/
     + k3s: https://k3s.io/
     + OpenShift: https://www.redhat.com/en/technologies/cloud-computing/openshift
     + VMWare Tanzu: https://tanzu.vmware.com/tanzu

   + Consume Kubernetes from a cloud provider

     + [Amazon (EKS)](https://aws.amazon.com/eks/)
     + [Google (GKE)](https://cloud.google.com/kubernetes-engine)
     + [Microsoft (AKS)](https://azure.microsoft.com/en-us/services/kubernetes-service)
     + [DigitalOcean (DOKS)](https://www.digitalocean.com/products/kubernetes/)

   + Tutorial:

     + Using Minikube to Create a Cluster:

       + https://kubernetes.io/docs/tutorials/kubernetes-basics/create-cluster/cluster-intro/

       + Create Kubernetes cluster

         + ```
           minikube start
           ```

       + View cluster details

         + ```
           $ kubectl cluster-info
           Kubernetes control plane is running at https://172.17.0.33:8443
           KubeDNS is running at https://172.17.0.33:8443/api/v1/namespaces/kube-system/services/kube-dns:dns/proxy
           
           To further debug and diagnose cluster problems, use 'kubectl cluster-info dump'.
           $ kubectl get nodes
           NAME       STATUS   ROLES                  AGE   VERSION
           minikube   Ready    control-plane,master   2m    v1.20.2
           ```

       + Deploy an app

         + ```
           $ kubectl create deployment kubernetes-bootcamp --image=gcr.io/google-samples/kubernetes-bootcamp:v1
           deployment.apps/kubernetes-bootcamp created
           $ kubectl get deployments
           NAME                  READY   UP-TO-DATE   AVAILABLE   AGE
           kubernetes-bootcamp   1/1     1            1           25s
           ```

       + View pods and nodes

         + ```
           kubectl get pods
           kubectl describe pods
           kubectl logs $POD_NAME
           ```

       + List the environment variables

         + ```
           $ kubectl exec $POD_NAME -- env
           PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
           HOSTNAME=kubernetes-bootcamp-fb5c67579-wjsb2
           KUBERNETES_PORT_443_TCP_ADDR=10.96.0.1
           KUBERNETES_SERVICE_HOST=10.96.0.1
           KUBERNETES_SERVICE_PORT=443
           KUBERNETES_SERVICE_PORT_HTTPS=443
           KUBERNETES_PORT=tcp://10.96.0.1:443
           KUBERNETES_PORT_443_TCP=tcp://10.96.0.1:443
           KUBERNETES_PORT_443_TCP_PROTO=tcp
           KUBERNETES_PORT_443_TCP_PORT=443
           NPM_CONFIG_LOGLEVEL=info
           NODE_VERSION=6.3.1
           HOME=/root
           ```

       + Execute commands in pods

         + ```
           kubectl exec -ti $POD_NAME -- bash
           ```

       + Expose your app

         + ```
           $ kubectl get services
           NAME         TYPE        CLUSTER-IP   EXTERNAL-IP   PORT(S)   AGE
           kubernetes   ClusterIP   10.96.0.1    <none>        443/TCP   2m59s
           $ kubectl expose deployment/kubernetes-bootcamp --type="NodePort" --port 8080
           service/kubernetes-bootcamp exposed
           ```

         + get cluster-IP, internal port and external-IP

           + ```
             $ kubectl get services
             NAME                  TYPE        CLUSTER-IP      EXTERNAL-IP   PORT(S)          AGE
             kubernetes            ClusterIP   10.96.0.1       <none>        443/TCP          18m
             kubernetes-bootcamp   NodePort    10.107.49.173   <none>        8080:30726/TCP   14m
             ```

         + check if we can get response from the server

           + ```
             curl $(minikube ip):$NODE_PORT
             ```

         + We can **use labels** like

           + ```
             kubectl get pods -l app=kubernetes-bootcamp
             kubectl get services -l app=kubernetes-bootcamp
             ```

           + apply a new label, example:

             + ```
               kubectl label pods $POD_NAME version=v1
               kubectl get pods -l version=v1
               ```

         + Delete service

           + ```
             kubectl delete service -l app=kubernetes-bootcamp
             ```

3. Demo: Kubernetes Setup

   + Goal in this part:

     + install 2 nodes Kubernetes cluster with kubeadm

   + https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/

   + Steps

     + Install container runtime

       + choices: Docker/ **containerd**/ CRI-O

     + Install kubeadm, kubelet, kubectl

       + `kubeadm`: the command to **bootstrap the cluster**
       + `kubelet`: the component that runs on all of the machines in your cluster and does things like **starting pods and containers**
       + `kubectl`: the command line util to **talk to your cluster**

     + kubeadm:

       + `kubeadm --help`

       + Create a two-machine cluster

         + control plane node: `kubeadm init`

           + output will give instructions for

             1. using the cluster

             2. deploy pod network

                + https://kubernetes.io/docs/concepts/cluster-administration/addons/

                + Calico: https://projectcalico.docs.tigera.io/getting-started/kubernetes/self-managed-onprem/onpremises

                  + Use Calico to install overlay network

                    + ```
                      curl https://projectcalico.docs.tigera.io/manifests/calico.yaml -O
                      kubectl apply -f calico.yaml
                      ```

             3. join worker nodes
                + `kubeadm join <arguments-returned-from-init>`

         + After these, we can check informations about the cluster

           + `kubectl get nodes`
           + `kubectl cluster-info`

4. Kubernetes API

   + Important component to **communication with the cluster**
   + Access Control Overview
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap4_AccessControlOverview.png)
     + 3 stages before a request is processed by Kubernetes:
       + Authentication
         + The requester needs to present a means of **identity** to authenticate against the API
       + Authorization
         + It is decided **what the requester is allowed to do**
       + Admission Control
         +  [admission controllers](https://kubernetes.io/docs/reference/access-authn-authz/admission-controllers/) can be used to **modify or validate the request**
   + Kubernetes API is implemented as a **RESTful** interface that is exposed over **HTTPS**
     + What user can do with the API:
       + **create**, **modify**, **delete** or **retrieve resources** that reside in Kubernetes

5. Running Containers on Kubernetes

   + Running a container on local machine VS Running containers in Kubernetes
     + Local machine:
       + Start container directly
     + Kubernetes:
       + Define **Pods** as the smallest compute unit ->Then Kubernetes translate that into a running container
       + When create a Pod in Kubernetes, several components are involved
         + Example using Docker:
           + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap4_ContainersInKubernetes.png)
         + Example using containerd:
           + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap4_ContainerCreationWithContainerd.png)
           + **much simpler than with Docker**
   + Container Runtimes Interface (CRI)
     + https://kubernetes.io/blog/2016/12/container-runtime-interface-cri-in-kubernetes/
     + Allow using other container runtimes than Docker
     + Container runtimes
       + containerd
         + **lightweight and performant** implementation to run containers
       + CRI-O
       + Docker
         + usage of Docker as the runtime for Kubernetes has been **deprecated** and will be removed in Kubernetes 1.23
   + Idea of containerd and CRI-O
     + provide a runtime that **only contains the absolutely essentials to run containers**
     + still have some other features:
       + Example: the ability to integrate with container runtime sandboxing tools (solve the security problem that comes with sharing the kernel between multiple containers)
         + [gvisor](https://github.com/google/gvisor)
         + [Kata Containers](https://katacontainers.io/)

6. Networking

   + Requirements
     + **All pods can communicate with each other across nodes.**
     + **All nodes can communicate with all pods.**
     + **No Network Address Translation (NAT)**
   + Network problems need to be solved:
     + **Container-to-Container communications**
       + solved by the Pod concept
     + **Pod-to-Pod communications**
       + solved by overlay network
     + **Pod-to-Service communications**
       + implemented by kube-proxy and packer filter on the node
     + **External-to-Service communications**
       + implemented by kube-proxy and packer filter on the node
   + Network vendors:
     + [Project Calico](https://www.tigera.io/project-calico/)
     + [Weave](https://www.weave.works/oss/net/)
     + [Cilium](https://cilium.io/)
   + In Kubernetes
     + every Pod gets it own IP address
     + most Kubernetes setups include a DNS server add-on ([core-dns](https://kubernetes.io/docs/tasks/administer-cluster/coredns/))
       + provide service discovery and name resolution inside the cluster

7. Scheduling

   + Requirement:
     + choose the right (worker) node to run a containerized workload on 
   + kube-scheduler
     + component that makes the scheduling decision 
       + but not responsible for starting the workload. Pod is started by **the kubelet and the container runtime**
     + How it works?
       + **user gives information about the application requirements**
         + including requests for CPU and memory and properties of a node
       + scheduler will filter all nodes that fit the requirements
         + multiple node fit the requirements -> select node with the least amount of Pods