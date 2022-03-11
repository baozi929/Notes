# Introduction

1. Chapter Overview

   + On-demand services
     + (virtual-) servers
     + networking
     + storage
     + databases
     + ......

2. Learning Objectives

   + Discuss the characteristics of Cloud Native Architecture.

   + Explain the benefits of autoscaling and serverless patterns.

   + Understand the importance of open standards and community projects in cloud native environments.



# Cloud Native Architecture

### Cloud Native Architecture Fundamentals

1. Core idea of cloud native architecture

   + Goal: optimize your software for cost efficiency, reliability and faster time-to-market
   + How: use a combination of cultural, technological and architectural designs

2. Definition of cloud native (by CNCF)

   + *“Cloud native technologies **empower organizations to build and run scalable applications in modern, dynamic environments** such as public, private, and hybrid clouds. **Containers, service meshes, microservices, immutable infrastructure, and declarative APIs** exemplify this approach.*

     *These techniques enable loosely coupled systems that are **resilient, manageable, and observable**. Combined with robust automation, they allow engineers to make high-impact changes frequently and predictably with minimal toil.* *[...]”*

3. Traditional applications VS Cloud native architecture

   + Traditional applications (Monolithic architecture)
     + self-contained and include all the functionality and component that are needed to fulfill a task
     + has a single code base and is provided as a single binary file that can run on a server
     + Limitations:
       + difficult to manage complexity, scale development across multiple teams, implement changes fast, and be able to scale the application out efficiently when it comes under a lot of load
   + Cloud native architecture
     + basic idea:
       + break down your application in smaller pieces -> more manageable
     + difference compared with traditional applications:
       + have multiple decoupled applications that communicate with each other in a network (the small, independent applications with a clearly defined scope of functions can be referred to as **microservices**)
       + -> able to operate and scale the services individually
   + Compare:
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap2_MonolithicvsMicroservices.png)

### Characteristics of Cloud Native Architecture

1. Characteristics of cloud native architecture
   + High level of automation
     + Recommendation:
       + automate all the moving parts of cloud native applicaiton (from development to deployment)
     + How:
       + using modern automation tools and Continuous Integration/Continuous Delivery (CI/CD) pipelines + version control system like git
     + Goal:
       + Deliver fast, frequent and incremental changes to production with minimal human involvement
         + works includes: building, testing and deploying applications & infrastructure
       + Disaster recovery
   + Self healing
     + Fact:
       + Applications & infrastructure fail from time to time
     + How to make service stable?
       + Health check
         + monitor applications from inside and automatically restart them if necessary
         + -> only some parts of the application stop working or get slower, but still other parts provide services
   + Scalable
     + What is scaling?
       + The process of handling more load while still providing a pleasant user experience
     + How?
       + Start multiple copies of the same application and distributing the load across them
         + Can be automated based on metrics like CPU or memory -> ensure availability and performance
   + (Cost-) Efficient
     + How to save costs:
       + When trafic is low:
         + Scale down both application and the usage-based pricing models of cloud providers
       + Orchestration systems like Kubernetes can help on more efficient and denser placing of applicaitons
   + Easy to maintain
     + Microservice -> break down applications in smaller pieces
       + -> more portable, easier to test and to distribute across multiple teams
   + Secure by default
     + Use different security models
       + Zero trust:
         + require authentication from every user and process
       + ......
2. The twelve-factor app
   + guideline for **developing cloud native applications**, such as
     + version control of codebase
     + environment-aware configuration
     + sophisticated patterns like statelessness and concurrency of your application
     + ......



### Autoscaling

1. What is autoscaling pattern?
   + dynamic adjust resources based on the current demand
     1. Use metrics (related to work load) like CPU, memory to decide on when to scale applications
     2. Can also use time or business metrics to scale the services up or down
2. Horizontal scaling VS Vertical scaling
   + Horizontal scaling
     + spawn new compute resources. Resources can be:
       + new copies of application process
       + virtual machines
       + new racks of servers and other hardware
   + Vertical scaling
     + the change in size of the underlying hardware
       + only works within certain hardware limits for bare machines or virtual machines
       + the **upper limit** is defined by the compute and memory capacity of the underlying hardware
         + example:
           + add more RAM -> limitation: number of RAM slots
   + Comparation
     + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap2_HorizontalvsVerticalscaling.png)
3. Configure autoscaling in environments
   + requires
     + configuring a minimum and maximum limit of instances
     + a metric that triggers the scaling
   + how to configure the correct scaling
     + need to:
       + run load test
       + analyze the behavior and load balancing
   + Autoscaling in cloud environments
     + On-demand pricing model
       + be able to provision a large amount of resources within seconds or even scale to zero

### Serverless

1. What is serverless
   + Serverless requires servers
   + Idea of serverless computing is that **developers only need to provide the application code**
     + cloud provider helps to choose right environment to run the application
       + (raise the complexity in operating a cloud platform)
2. Function as a Service (FaaS)
   + cloud provider abstract the underlying infrastructure -> developers can deploy software by uploading code or providing a container image
   + **Autoscaling is an important core concept of serverless**
     + includes scaling and provisioning based on events like incoming requests
     + -> more precise billing (based on events instead of the widespread time-based billing)
   + Usage scenario：
     + used in combination or as an extension of existing platforms
       + allow fast deployment
       + make for excellent testing and sandbox environments
   + Standardization
     + why?
       + difficult to switch between different platforms
     + the [CloudEvents](https://cloudevents.io/) project
       + provides a specification of how event data should be structured
         + Events are the basis for scaling serverless workloads or triggering corresponding functions
3. Applications on serverless platforms
   + stricter requirements for cloud native architecture
   + writing **small, stateless applications** make them a perfect fit for event or data streams, scheduled tasks, business logic or batch processing

### Open Standards

1. Common problem
   + How to build and distribute software packages?
     + Pain point:
       + applications have a lot of **requirements and dependencies for the underlying operating system and the application runtime**
     + Solution:
       + Container
   + Standards for how to build and run containers
     + [Open Container Initiative (OCI)](https://opencontainers.org/)
       + [image-spec](https://github.com/opencontainers/image-spec)
         + defines how to build and package container images
       + [runtime-spec](https://github.com/opencontainers/runtime-spec)
         + specifies the configuration, execution environment and lifecycle of containers
       + [Distribution-Spec](https://github.com/opencontainers/distribution-spec)
         + provides a standard for the distribution of content in general and container images in particular
2. Standards in next chapters
   + [OCI Spec](https://opencontainers.org/):
     + includes:
       + image-spec
       + runtime-spec
       + distribution-spec
     + defines how to run, build an distribute containers
   + [Container Network Interface (CNI)](https://github.com/containernetworking/cni):
     + A specification on how to **implement networking** for Containers.
   + [Container Runtime Interface (CRI)](https://github.com/kubernetes/cri-api):
     + A specification on how to **implement container runtimes** in container orchestration systems.
   + [Container Storage Interface (CSI)](https://github.com/container-storage-interface/spec):
     + A specification on how to **implement storage** in container orchestration systems.
   + [Service Mesh Interface (SMI)](https://smi-spec.io/):
     + A specification on how to **implement Service Meshes** in container orchestration systems with a focus on Kubernetes.



### Cloud Native Roles & Site Reliability Engineering

1. Job roles in cloud computing
   + Cloud Architect
     + adopt cloud technologies
     + design application landscape and infrastructure (focus on security, scalability and deployment mechanisms)
   + DevOps Engineer
     + use tools and processes that balance out software development and operations
   + Security Engineer
   + DevSecOps Engineer
     + DevOps Engineer + Security Engineer
   + Data Engineer
     + collect, store, and analyze the vast amounts of data that are being or can be collected in large systems
       + provisioning and managing specialized infrastructure
       + working with that data
   + Full-Stack Developer
     + frontend and backend development + infrastructure essentials
   + Site Reliability Engineer (SRE)
     + Goal of SRE:
       + create and maintain software that is **reliable and scalable**
     + SREs use 3 main metrics to measure performance and reliability
       + **Service Level Objectives (SLO)**:
         + “*Specify a **target level** for the reliability of your service.”* 
         + For example, reaching a service latency of less that 100ms.
       + **Service Level Indicators (SLI)**:
         + *“A carefully defined **quantitative measure** of some aspect of the level of service that is provided”*
         + For example how long a request actually needs to be answered.
       + **Service Level Agreements (SLA)**:
         + “***An explicit or implicit contract with your users that includes consequences of meeting (or missing) the SLOs they contain**. The consequences are most easily recognized when they are financial – a rebate or a penalty – but they can take other forms.”* 
         + Answers the question what happens if SLOs are not met.



### Community and Governance

1. Cloud Native Computing Foundation (CNCF)
   + hosts and supports many open source projects that are seen as industry standard
   + Technical Oversight Committee (TOC) in CNCF
     + is responsible for:
       + define and maintain the technical vision
       + approve new projects
       + accept feedback from the end-user committee
       + define common practices that should be implemented in CNCF projects
     + practices the principle of “**minimal viable governance**”
       + does not control the projects
       + but encourages them to be self-governing and community owned



### Additional Resources

1. Cloud Native Architecture

   - [Adoption of Cloud-Native Architecture, Part 1: Architecture Evolution and Maturity](https://www.infoq.com/articles/cloud-native-architecture-adoption-part1/), by Srini Penchikala, Marcio Esteves, and Richard Seroter (2019)

   - [5 principles for cloud-native architecture-what it is and how to master it](https://cloud.google.com/blog/products/application-development/5-principles-for-cloud-native-architecture-what-it-is-and-how-to-master-it), by Tom Grey (2019)

   - [What is cloud native and what are cloud native applications?](https://tanzu.vmware.com/cloud-native)

   - [CNCF Cloud Native Interactive Landscape](https://landscape.cncf.io/)

2. Well-Architected Framework

   - [Google Cloud Architecture Framework](https://cloud.google.com/architecture/framework)

   - [AWS Well-Architected Framework](https://docs.aws.amazon.com/wellarchitected/latest/framework/welcome.html)

   - [Microsoft Azure Well-Architected Framework](https://docs.microsoft.com/en-us/azure/architecture/framework/)

3. Microservices

   - [What are microservices?](https://microservices.io/)

   - [Microservices](https://martinfowler.com/articles/microservices.html), by James Lewis and Martin Fowler

   - [Adopting Microservices at Netflix: Lessons for Architectural Design](https://www.nginx.com/blog/microservices-at-netflix-architectural-best-practices/)

4. Serverless

   - [The CNCF takes steps toward serverless computing](https://www.cncf.io/blog/2018/02/14/cncf-takes-first-step-towards-serverless-computing/), by Kristen Evans (2018)

   - [CNCF Serverless Whitepaper v1.0](https://www.google.com/url?q=https://github.com/cncf/wg-serverless/tree/master/whitepapers/serverless-overview&sa=D&source=docs&ust=1636466570370000&usg=AOvVaw1xOr0iKDTSiS3io2WmyrQJ) (2019)

   - [Serverless Architecture](https://cloud.google.com/serverless/whitepaper)

5. Site Reliability Engineering

   - [SRE Book](https://sre.google/sre-book/introduction/), by Benjamin Treynor Sloss (2017)

   - [DevOps, SRE, and Platform Engineering](https://iximiuz.com/en/posts/devops-sre-and-platform-engineering/), by Ivan Velicho (2021)