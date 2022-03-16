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

### Use of Containers

1. Challenge:
   + **Developer** knows his applications and its dependencies best
   + **System administrator** provides the infrastructure, installs all of the dependencies, and configures the system on which the application runs
     + **Error-prone** and **hard to maintain**
2. Solutions

   + Virtual machines

     + can be used to emulate a full server with **cpu, memory, storage, networking, operating system and the software** on top
     + Pros:
          + allows **running multiple isolated servers** on the same hardware
     + Cons:
       + **Need a whole operating system including the kernel**, therefore comes with some **overhead**

   + Containers
     + Pros:
       
       + managing the **dependencies of an application**
         + running **much more efficiently** than spinning up a lot of virtual machines



### Container Basics

1. Ancestors of modern container technologies
   + `chroot`:  introduced in Version 7 Unix in 1979
     + could be used to **isolate a process from the root filesystem** and basically "hide" the files from the process and **simulate a new root directory**
     
     + Example:
       
       + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_chroot.png)

2. Current ways to isolate a process

   + `namespaces`
     + used to **isolate various resources**
       
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
         + example:
           + limit application container to 4GB of memory
   
3. **Virtual machines** VS **Containers**

   + Virtual machines
     + emulate a complete machine, including the operating system and a kernel -> **overhead** (boot time, size or resource usage to run the OS)
     + security advantages: **greater isolation**
   + Containers
     + share the kernel of the host machine and are only **isolated processes** -> **faster and have a smaller footprint**

   + Traditional VS Virtualized VS Container
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_TraditionalvsVirtualizedvsContainer.png)



### Running Containers

1. To run industry-standard container
   + Use Docker
   + Follow OCI [runtime-spec](https://github.com/opencontainers/runtime-spec)
     + describes how to
       + unpack a container image
       + then manage the complete container lifecycle, from creating the container environment, to starting the process, stopping and deleting it
     + [runC](https://github.com/opencontainers/runc) is a container runtime reference implementation
     + Open Container Initiative (OCI) includes image-spec, runtime-spec and distribution-spec

2. Relationship between image and container
   + similar to a class (image) and instantiation of the class (container)

3. Tools
   + build images: [buildah](https://buildah.io/) or [kaniko](https://github.com/GoogleContainerTools/kaniko)
   + alternative to Docker: [podman](https://podman.io/) (provides similar API as Docker)

4. Demo: Running Containers

   + Docker
     
     + Install: [Install Docker Engine | Docker Documentation](https://docs.docker.com/engine/install/)
     
     + Images: https://hub.docker.com/
     
     + Commands:
       
       + pull images: `docker pull nginx`
       
       + check local images: `docker images`
       
       + create container and run: `docker run --detach --publish-all nginx:latest`
         
         + --detach, -d: Run container in background and print container ID
         
         + --publish, -p: `localport:containerport`, publish a container's port(s) to the host
         
         + --publish-all, -P: Publish all exposed ports to random ports
       
       + check containers: `docker ps`
       
       + stop container: `docker stop CONTAINER_ID`


   + Podman
     
     + Install: https://podman.io/getting-started/installation



### Building Container Images

1. Docker

   + Basic functions: 
     + isolate processes (Reuse components like namespaces and cgroups)

   + **Breakthrough: container images**
     + Advantage: makes containers **portable** and **easy to reuse** on a variety of systems

     + Definition:
       
       + *A Docker container image is a **lightweight, standalone, executable package** of software that includes everything needed to run an application: **code, runtime, system tools, system libraries and settings**.*

     + OCI image-spec: https://github.com/opencontainers/image-spec

     + Image = a filesystem bundle + metadata

2. Container Images

   + How to build？
     
     + reading the instructions from a buildfile called **Dockerfile**
     
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
     
       + --tag, -t: can specify a name tag for your image, like **name:tag**
       
       + -f: specify path of Dockerfile
       
     
   + Use container registry to distribute images:
     
     + ```shell
       docker push my-registry.com/my-python-image
       docker pull my-registry.com/my-python-image
       ```

3. Demo: Building Container images
   + [Sample application | Docker Documentation](https://docs.docker.com/get-started/02_our_app/)



### Security

1. Security requirement for containers and virtual machines are different
   + **Rely on the isolation properties of containers can be very dangerous**

2. Characteristics of containers:
   + Share the same kernel -> risky for the whole system
     + Example:
       + containers might be allowed to call kernel functions like:
         + killing other processes
         + modifying the host network by creating routing rules
     + [Docker security | Docker Documentation](https://docs.docker.com/engine/security/#linux-kernel-capabilities)

3. Security risks

   + Execute processes with **too many privileges** (especially start processes as **root** or **administrator**)

   + Use of public images
     + Popular public image registries:
       
       + Docker Hub: https://hub.docker.com/
       + Quay: https://quay.io/
     + Need to make sure that these images were not modified to include malicious software

4. How to avoid security issues and build secure container images:
   + [Top 20 Dockerfile best practices for security &ndash; Sysdig](https://sysdig.com/blog/dockerfile-best-practices/)

5. The **4C**'s of **Cloud Native security** can give a rough idea **what layers need to be protected** if you’re using containers

   + 4C
     + Code
     
     + Container
     
     + Cluster
     
     + Could
     
+ Reference: [Kubernetes documentation](https://kubernetes.io/docs/concepts/security/overview/)



### Container Orchestration Fundamentals

1. The basis of **microservice** architecture: 
   + a lot of small containers that are **loosely coupled, isolated and independent**
     + small containers are **self-contained small parts of business logic** that are part of a bigger application


2. Problems to be solved when managing and deploying large amounts of containers:

   + Providing **compute resources** like virtual machines where containers can run on
   
   
      + **Schedule** containers to servers in an efficient way
   
   
      + **Allocate resources like CPU and memory** to containers
   
   
      + **Manage the availability of containers** and replace them if they fail
   
   
      + **Scale containers** if load increases
   
   
      + **Provide networking** to connect containers together
   
   
      + **Provision storage** if containers need to **persist data**
   

3. Most **container orchestration systems** consist of two parts:

   + a **control plane** that is responsible for the management of the containers 

   + **worker nodes** that actually host the containers



### Networking

1. Microservice architecture VS Monolithic applications

   + Microservice architecture depends heavily on network communication
     + microservice implements an **interface that can be called to make a request**

2. Network in containers

   + **Network namespace** allows each container can have it's own **unique IP address**
     + -> **multiple applications can open the same network port** (example: can have multiple contianerized web server with port 8080)

   + Map a port form the container to a port form the host system -> make **application accessible from ouside the host system**

   + Use an **overly network** (puts them in a virtual network that is spanned across the host systems) -> **containers across host communicate with each other**

     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_RoutingBetweenHostsAndContainers.png)
     + benefits:
       + easy for containers to communicate with each other (administrators don't need to configure complex networking and routing between hosts and containers)

3. [Container Network Interface (CNI)](https://github.com/containernetworking/cni)

   + What is CNI:
     + A standard that can be used to **write or configure network plugins**

   + Benefits form CNI:
     + easy to swap out different plugins in various container orchestration platforms



### Service Discovery & DNS

1. Traditional ways to manage servers in data centers
   + administrators **manually maintain** large lists of servers, their host names, IP addresses, and purposes

2. Challenges and solutions in container orchestration platforms:

   + New challenges:
     
     + Hundreds or thousands of containers with individual IP addresses
     
     + Containers are deployed on varieties of different hosts, different data centers or even geolocations
     
     + Containers or Services need DNS to communicate. Using IP addresses is nearly impossible
     
     + Information about Containers must be removed from the system when they get deleted


   + Solution: Automation
     
     + Put all the information in a **Service Registry**
     
     + **Service Discovery**
       + Definition: **Finding other services in the network and requesting information about them**
       + The most used approaches:
         
         + DNS
           
           + DNS server have a **service API**
             
             + can be used to **register new services** as they are created
         + K-V Store
           
           + Using a strongly consistent datastore **especially to store information about services**
           + Popular choices for clustering:
             
             + etcd: https://github.com/coreos/etcd/
             + Concul: https://www.consul.io/
             + Apache Zookeeper: https://zookeeper.apache.org/



### Service Mesh

1. Challenge:

   + Network is important for microservices and containers -> **networking can be complex and opaque for developers and administrators**


   + Functionality need when **container communicate with each other**:
     + monitoring
     + access control
     + encryption of the networking traffic

2. Solutions

   + Solution 1: Proxy
     + Start **a second container** to manage network traffic instead of implement all the functionalities to your app
       
       + **proxy**, sits between a client and server, can modify or filter network traffic
       
       + popular:
         
         + nginx: https://www.nginx.com/
         
         + haproxy: http://www.haproxy.org/
         
         + envoy: https://www.envoyproxy.io/

   + Solution 2: Service mesh

     + What service mash does?
       + **adds a proxy server to *every* container** that you have in your architecture
     + Istio Architecture:
       + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_IstioArchitecture.png)

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

3. Example for service mesh:
   + Encryption
     + Requirements:
       + two or more applications want to encrypt their traffic when they talk to each other
     + Traditional solution:

       + requirements:
         + add Libraries, configuration and management of digital certificates
       + **limitations:**
         + **a lot of work, error-prone**
     + Service mesh solution:

       + How it works?
         + **traffic routed through the proxies** (applications don't talk to each other directly)
       + popular service mesh:

         + istio: https://istio.io/

         + linkerd: https://linkerd.io/

4. Standard for service mesh: [Service Mesh Interface (SMI)](https://smi-spec.io/) project

   + aims at defining a specification on **how a service mesh from various providers can be implemented**

   + goal:
     
     + standardize the end user experience for service meshes
     + a standard for the providers that want to **integrate with Kubernetes**



### Storage

1. Container:

   + Flaw from a storage perspective: **ephemeral**

   + How it works when container get started

     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_ContainerLayers.png)

     + **Image layer**:
       + read-only
       + consist of layers that include what you added during the build phase
         + -> get the same behavior and functionality every time
       
     + **read-write layer**:
       + **lost when the container is stopped or deleted**
       
       + -> to persist data, need to **write to disk**
   
2. Challenges:

   1. **Persist data** on the host where container was started
      + How to persist data
        
        + Use container **volumns** to have access to the host filesystem
          + directories that reside on the host are passed through into the container filesystem (instead of isolating the whole filesystem of a process)
        + Figure: Data shared between 2 containers on the same host
          + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_DataIsSharedBetween2ContainersOnTheSameHost.png)

   2. Containers started on different hosts should have access to its volume
      + Example:
        + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap3_Storage.png)
        + Storage: provisioned via a central storage system
        + Containers on Server A and Server B: share Storage Volume 1 to read and write data
        + Containers on Server C: use Storage Volume 2

3. Standard for storage implementations
   + Container Storage Interface (CSI): https://github.com/container-storage-interface/spec
     
     + offer a **uniform interface** which allows attaching different storage systems no matter if it’s **cloud or on-premises** storage



### Additional Resources

1. The History of Containers

   - [A Brief History of Containers: From the 1970s Till Now](https://blog.aquasec.com/a-brief-history-of-containers-from-1970s-chroot-to-docker-2016), by Rani Osnat (2020)

   - [It's Here: Docker 1.0](https://web.archive.org/web/20160426102954/https://blog.docker.com/2014/06/its-here-docker-1-0/), by Julien Barbier (2014)

2. Chroot
   - [chroot](https://wiki.ubuntuusers.de/chroot/)

3. Container Performance
   - [Container Performance Analysis at DockerCon 2017](https://www.brendangregg.com/blog/2017-05-15/container-performance-analysis-dockercon-2017.html), by Brendan Gregg

4. Best Practices on How to Build Container Images

   - [Top 20 Dockerfile Best Practices](https://sysdig.com/blog/dockerfile-best-practices/), by Álvaro Iradier (2021)

   - [3 simple tricks for smaller Docker images](https://learnk8s.io/blog/smaller-docker-images), by Daniele Polencic (2019)

   - [Best practices for building containers](https://cloud.google.com/architecture/best-practices-for-building-containers)

5. Alternatives to Classic Dockerfile Container Building
   - [Buildpacks vs Jib vs Dockerfile: Comparing containerization methods](https://trainingportal.linuxfoundation.org/learn/course/kubernetes-and-cloud-native-essentials-lfs250/container-orchestration/Ál), by James Ward (2020)

6. Service Discovery
   - [Service Discovery in a Microservices Architecture](https://www.nginx.com/blog/service-discovery-in-a-microservices-architecture/), by Chris Richardson (2015)

7. Container Networking

   - [Kubernetes Networking Part 1: Networking Essentials](https://www.inovex.de/de/blog/kubernetes-networking-part-1-en/), By Simon Kurth (2021)

   - [Life of a Packet (I)](https://www.youtube.com/watch?v=0Omvgd7Hg1I), by Michael Rubin (2017)

   - [Computer Networking Introduction - Ethernet and IP (Heavily Illustrated)](https://iximiuz.com/en/posts/computer-networking-101/), by Ivan Velichko (2021)

8. Container Storage

- [Managing Persistence for Docker Containers](https://thenewstack.io/methods-dealing-container-storage/), by Janakiram MSV (2016)

9. Container and Kubernetes Security
   - [Secure containerized environments with updated thread matrix for Kubernetes](https://www.microsoft.com/security/blog/2021/03/23/secure-containerized-environments-with-updated-threat-matrix-for-kubernetes/), by Yossi Weizman (2021)

10. Docker Container Playground
    - [Play with Docker](https://labs.play-with-docker.com/)
