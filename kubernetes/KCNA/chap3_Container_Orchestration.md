# Introduction

1. Overview
   
   + challenges and opportunities of container orchestration
   
   + why it has special requirements regarding networking and storage
   
   + what a container is and what makes it so useful in building and operating applications

2. Learning objectives
   
   + Explain what a **container** is and how it differs from **virtual machines**.
   
   + Describe the fundamentals of **container orchestration**.
   
   + Discuss the challenges of **container networking and storage**.
   
   + Explain the **benefits of a service mesh**.

# Container Orchestration

1. Use of Containers
   
   + Challenge:
     
     + **Developers** knows his applications and its dependencies best, but it's a **system administrator** who provides the infrastructure, installs all of the dependencies, and configures the system on which the application runs
       
       + **Error-prone** and **hard to maintain**
   
   + Solutions
     
     1. Virtual machines
        
        + can be used to emulate a full server with **cpu, memory, storage, networking, operating system and the software** on top
        
        + Pros:
          
          + allows **running multiple isolated servers** on the same hardware
        
        + Cons:
          
          + **Need a whole operating system including the kernel**, therefore comes with some **overhead**
     
     2. Containers
        
        + Pros:
          
          + managing the **dependencies of an application**
          
          + running **much more efficiently** than spinning up a lot of virtual machines

2. Container Basics
   
   + Ancestors of modern container technologies
     
     + `chroot`:  introduced in Version 7 Unix in 1979
       
       + could be used to **isolate a process from the root filesystem** and basically "hide" the files from the process and **simulate a new root directory**
       
       + Example:
         
         + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chroot.png)
   
   + Current ways to isolate a process
     
     + `namespaces`
       
       + used to isolate various resources
         
         + example: 
           
           + **network namespace** can be used to provide a complete abstraction of network interfaces and routing tables
             
             + therefore, a process will have **its own IP address**
       
       + Linux Kernel 5.6 currently provides 8 namespaces
         
         + `pid` - process ID, provides a process with its **own set of process IDs**
         
         + `net` - network, allows the processes to have their **own network stack**, including the IP address
         
         + `mnt` - mount, abstracts the **filesystem view** and manages **mount points**
         
         + `ipc` -  inter-process communication, provides separation of named shared memory segments
         
         + `user` -  provides process with their **own set of user IDs and group IDs**
         
         + `uts` - Unix time sharing, allows processes to have their **own hostname and domain name**
         
         + `cgroup` - allows a process to have its **own set of cgroup root directories**
         
         + `time` - can be used to **virtualize the clock of the system**
     
     + `cgroups`
       
       + used to
         
         + **organize processes in hierarchical groups**
         
         + **assign them resources** like memory and CPU
   
   + **Virtual machines** VS **Containers**)
     
     + Virtual machines
       
       + emulate a complete machine, including the operating system and a kernel -> **overhead** (boot time, size or resource usage to run the OS)
       
       + security advantages: **greater isolation**
     
     + Containers
       
       + share the kernel of the host machine and are only **isolated processes** -> **faster and have a smaller footprint**
     
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/TraditionalvsVirtualizedvsContainer.png)

3. Running Containers
   
   + Standard:
     
     + Open Container Initiative (OCI)
       
       + image-spec
       
       + runtime-spec
       
       + distribution-spec
   
   + Example：
     
     + After Docker installed, command to start container:
       + ```shell
         docker run nginx
         ```

4. Demo: Running Containers
   
   + Docker
     
     + Install: [Install Docker Engine | Docker Documentation](https://docs.docker.com/engine/install/)
     
     + Images: https://hub.docker.com/
     
     + Commands:
       
       + pull images: `docker pull nginx`
       
       + check local images: `docker images`
       
       + create container and run: `docker run --detach --publish-all nginx:latest`
         
         + --detach, -d: Run container in backaground and print container ID
         
         + --publish, -p: `localport:containerport`, publish a container's port(s) to the host
         
         + --publish-all, -P: Publish all exposed ports to random ports
       
       + check containers: `docker ps`
       
       + stop container: `docker stop CONTAINER_ID`
   
   + Podman
     
     + Install: https://podman.io/getting-started/installation

5. Building Container Images
   
   + Docker
     
     + Basic functions: isolate processes (Reuse components like namespaces and cgroups)
     
     + **Breakthrough: container images**
       
       + Advantage: makes containers **portable** and **easy to reuse** on a variety of systems
       
       + Definition:
         
         + *A Docker container image is a **lightweight, standalone, executable package** of software that includes everything needed to run an application: **code, runtime, system tools, system libraries and settings**.*
       
       + OCI image-spec: https://github.com/opencontainers/image-spec
       
       + Image: a filesystem bundle + metadata
   
   + Container Images
     
     + How to build？
       
       + reading the instructions from a buildfile called *Dockerfile*
       
       + Dockerfile example (containerizes a Python script)
         
         + ```dockerfile
           # Every container image starts with a base image.
           # This could be your favorite linux distribution
           FROM ubuntu:20.04 
           
           # Run commands to add software and libraries to your image
           # Here we install python3 and the pip package manager
           RUN apt-get update && \
               apt-get -y install python3 python3-pip 
           
           # The copy command can be used to copy your code to the image
           # Here we copy a script called "my-app.py" to the containers filesystem
           COPY my-app.py /app/ 
           
           # Defines the workdir in which the application runs
           # From this point on everything will be executed in /app
           WORKDIR /app
           
           # The process that should be started when the container runs
           # In this case we start our python app "my-app.py"
           CMD ["python3","my-app.py"]
           ```
     
     + Command to build image
       
       + ```shell
         docker build -t my-python-image -f Dockerfile
         ```
       
       + -t: can specify a name tag for your image, like **name:tag**
       
       + -f: specify path of Dockerfile
     
     + Use container registry to distribute images:
       
       + ```shell
         docker push my-registry.com/my-python-image
         docker pull my-registry.com/my-python-image
         ```

6. Demo: Building Container images
   
   + [Sample application | Docker Documentation](https://docs.docker.com/get-started/02_our_app/)

7. Security
   
   + **Rely on the isolation properties of containers can be very dangerous**
   
   + Characteristics of containers:
     
     + Share the same kernel -> risky for the whole system
       
       + [Docker security | Docker Documentation](https://docs.docker.com/engine/security/#linux-kernel-capabilities)
   
   + Security risks
     
     1. Execute processes with too many privileges (especially start processes as **root** or **administrator**)
     
     2. Use of public images
        
        + Popular public image registries:
          
          + Docker Hub: https://hub.docker.com/
          
          + Quay: https://quay.io/
   
   + Avoid security issues and build secure container images:
     
     + [Top 20 Dockerfile best practices for security &ndash; Sysdig](https://sysdig.com/blog/dockerfile-best-practices/)
   
   + The **4C**'s of **Cloud Native security** can give a rough idea **what layers need to be protected** if you’re using containers
     
     + 4C
       
       + Code
       
       + Container
       
       + Cluster
       
       + Could
     
     + [Kubernetes documentation](https://kubernetes.io/docs/concepts/security/overview/)

8. Container Orchestration Fundamentals
   
   + The basis of **microservice** architecture: 
     
     + a lot of small containers that are **loosely coupled, isolated and independent**
     
     + small containers are **self-contained small parts of business logic** that are part of a bigger application
   
   + Problems to be solved when managing and deploying containers:
     
     + Providing **compute resources** like virtual machines where containers can run on
     
     + **Schedule** containers to servers in an efficient way
     
     + **Allocate resources like CPU and memory** to containers
     
     + **Manage the availability of containers** and replace them if they fail
     
     + **Scale containers** if load increases
     
     + **Provide networking** to connect containers together
     
     + **Provision storage** if containers need to **persist data**
   
   + Most **container orchestration systems** consist of two parts:
     
     + a **control plane** that is responsible for the management of the containers 
     
     + **worker nodes** that actually host the containers

9. Networking
   
   + Each container can have it's own unique IP address -> **multiple applications can open the same network port**
     
     + network namespace
   
   + Map a port form the container to a port form the host system -> make **application accessible from ouside the host system**
   
   + Use an **overly network** (puts them in a virtual network that is spanned across the host systems) -> **containers accross host communicate with each other**
     
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/RoutingBetweenHostsAndContainers.png)
   
   + Standard for **container networking**: [Container Network Interface (CNI)](https://github.com/containernetworking/cni)
     
     + used to write or configure network plugins
     
     + easy to swap out different plugins in various container orchestration platforms

10. Service Discovery & DNS
    
    + Traitional ways
      
      + manual maintain large lists of servers, their host names, IP addresses, and purposes
    
    + In container orchestration:
      
      + Challanges
        
        + Hundreds or thousands of containers with individual IP addresses
        
        + Containers are deployed on varieties of different hosts, different data centers or even geolocations
        
        + Containers or Services need DNS to communicate. Using IP addresses is nearly impossible
        
        + Information about Containers must be removed from the system when they get deleted
      
      + Solution: Automation
        
        + Put all the information in a **Service Registry**
    
    + **Service Discovery**
      
      + Definition: Finding other services in the network and requesting information about them
      
      + Approaches:
        
        + DNS
          
          + DNS server have a **service API**
            
            + can be used to **register new services** as they are created
        
        + K-V Store
          
          + Using a strongly consistent datastore **especially to store information about services**
          
          + Popular choices for clustering:
            
            + etcd: https://github.com/coreos/etcd/
            
            + Concul: https://www.consul.io/
            
            + Apache Zookeeper: https://zookeeper.apache.org/

11. Service Mesh
    
    + Challange:
      
      + Network is important for microservices and containers -> can be complex and opaque for developers and administrators
      
      + Functionalility need when **container communicate with each other**: monitoring, access control, encryption of the networking traffic
    
    + Solution 1: Proxy
      
      + Start a second container to manage network traffic instead of implement all the functionalities to your app
        
        + **proxy**, sits between a client and server, can modify or filter network traffic
        
        + popular:
          
          + nginx: https://www.nginx.com/
          
          + haproxy: http://www.haproxy.org/
          
          + envoy: https://www.envoyproxy.io/
    
    + Solution 2:
      
      + a service mesh **adds a proxy server to *every* container** that you have in your architecture
      
      + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/IstioArchitecture.png)
        
        + data plane: formed by the proxies in a service
          
          + implement networking rules
          
          + shape the traffic flow
        
        + control plane: manage the rules, such as:
          
          + how traffic flows from service A to service B
          
          + what configuration should be applied to the proxies
      
      + Advantage compared with solution 1:
        
        + No need to write code and install librarys -> **only need to write a config file**
          
          + example:
            
            + write a config file can tell the service mesh that service A and service B should always communicate encrypted
            
            + then upload it to control plane and distribute to data plane to enforce the rules
    
    + Example:
      
      + Encrytion
        
        + Traditional solution:
          
          + 2 or more apps encrypt their traffic when they talk to each other
          
          + requirements: Librarys, configuration, management of digital certificates
          
          + **limitations: a lot of work, error-prone**
        
        + Service mesh solution:
          
          + traffic routed through the proxies
          
          + popular:
            
            + istio: https://istio.io/
            
            + linkerd: https://linkerd.io/
    
    + Service Mesh Interface (SMI) project
      
      + aims at defining a specification on **how a service mesh from various providers can be implemented**
      
      + goal:
        
        + standardize the end user experience for service meshes
        
        + a standard for the providers that want to integrate with Kubernetes

12. Storage
    
    + Container Layers:
      
      + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/ContainerLayers.png)
      
      + Image layer:
        
        + read-only
        
        + consist of layers that include what you added during the build phase
      
      + read-write layer
        
        + **lost when the container is stopped or deleted**
        
        + -> to persist data, need to **write to disk**
    
    + Challenges:
      
      1. **Persist data** on the host where container was started
         + How to persist data
           
           + Use container **volumns** to have access to the host filesystem
           
           + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/DataIsSharedBetween2ContainersOnTheSameHost.png)
      2. Containers started on different hosts should have access to its volume
         + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/Storage.png)
         
         + Example: Central storage system
    
    + Standard for storage implementations
      
      + Container Storage Interface (CSI): https://github.com/container-storage-interface/spec
        
        + offer a **uniform interface** which allows attaching different storage systems no matter if it’s **cloud or on-premises** storage
