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
     
     + ![]()



3. Running Containers
   
   + 

4. 

5. 
