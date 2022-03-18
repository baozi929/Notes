# Introduction

1. Chapter Overview
   + Change of methods to **deploy an application on different platforms**
     1. execute on the same machine they were written on
     2. transported by physical medium (floppy disk, USB, CD)
     3. check in code on a server, put it in a container, and deploy it to a platform like Kubernetes
   + Most important change:
     + automation of the deployment process
       + this allows very fast, more frequent and higher quality software deployments
   + **Continuous Integration/Continuous Delivery (CI/CD)** and how they further advanced in new tools and practices like GitOps
2. Learning Objectives
   + Discuss the **importance of automation in integration and delivery of applications**
   + Understand the need for Git and version control systems
   + Explain **what a CI/CD pipeline is**.
   + Discuss the idea of **Infrastructure as Code** (IaC)
   + Discuss the principles of GitOps and how it integrates with Kubernetes



# Cloud Native Application Delivery

### Application Delivery Fundamentals

1. Steps to deliver applications

   + Step1: Source Code
     + The best way to manage source code: **Version control**
       + Version control tool: Git
   
   
      + Step2: **Build source code**
        + could also include the building of a **container image**
   
   
      + Step3: **Automation test** of the app to make sure all functionality still works fine after code change
   
   
      + Step4: **Deliver the application to the platform** it should run on
        + If target platform is Kubernetes, need YAML file to deploy the app -> build container image and push to a container registry
   

2. **Source code is not the only thing managed in a version control system**
   + **Infrastructure as Code** (IaC): https://en.wikipedia.org/wiki/Infrastructure_as_code
     + No need to install infrastructure manually
       + -> describe it in files and **use the cloud vendors' API to setup the infrastructure**



### CI/CD

1. Requirements:

   + service get smaller

   + deployments get more frequent
     + manual steps are error-prone

2. Solution:
   + **Automation of the deployment process**
     + use the principles of **Continuous Integration/Continuous Delivery (CI/CD)**, which describe the different steps in the deployment of an application, configuration or even infrastructure
     + Part1: Continuous Integration (CI)
       + includes permanent **building and testing** of the written code
         + **high automation + version control** allows multiple teams work on the same code base
     + Part2: Continuous Delivery (CD)
       + automates the **deployment of the pre-built software**
         + in cloud environments, software is often deployed to Development or Staging environments before it released and delivered to a production system
     + -> To automate the whole workflow -> **CI/CD pipeline**
       + the scripted form of all the steps involved, running on a server or even in a container
       + What the pipeline should include:
         + version control system -> to manage changes to the code base
         + execute scripts to build the code, run tests, deploy them to servers (and even perform security and compliance checks) **every time a new version of the code is ready to be deployed**

3. Popular CI/CD tools:
   + [Spinnaker](https://spinnaker.io/)
     + [GitLab](https://gitlab.com/#)
     + [Jenkins](https://www.jenkins.io/)
     + [Jenkins X](https://jenkins-x.io/)
     + [Tekton CD](https://github.com/tektoncd/pipeline)
     + [Argo](https://argoproj.github.io/)
     + Open source tools for Kubernetes to run workflows, manage clusters, and do GitOps right.

4. [Introduction to DevOps and Site Reliability Engineering (LFS162)](https://training.linuxfoundation.org/training/introduction-to-devops)



### GitOps

1. Profit from Infrastructure as Code

   + increasing the **quality and speed** of providing infrastructure

   + Includes:
     + **Infrastructure** as code
     + **Configuration** as code
     + **Network** as code
     + **Policies**  as code
     + **Security** as code
2. GitOps

   + takes the idea of **Git as the single source of truth** a step further
     + includes infrastructure, configurations, ... to describe an environment
   + **integrates the provisioning and change process of infrastructure with version control operations**
   + merge requests are used to manage infrastructure changes
3. 2 approaches how a CI/CD pipeline can implement the changes you want to make:

   + Reference: 
     + https://blog.csdn.net/xixihahalelehehe/article/details/122193489
     + https://zhuanlan.zhihu.com/p/423065573
     + https://zhuanlan.zhihu.com/p/431562168
   + Push-based
     + The pipeline is started and runs tools that make the changes in the platform. **Changes can be triggered by a commit or merge request**.
       + Tools:
         + [Jenkins](https://www.jenkins.io/)
         + [CircleCI](https://circleci.com/)
         + [Travis CI](https://www.lambdatest.com/automate-selenium-tests-with-travisci?utm_source=google&utm_medium=cpc&utm_campaign=[HOP] Test Automation APAC&utm_term=&utm_id=12741819968&gclid=CjwKCAiAiKuOBhBQEiwAId_sK3RqSKJRWIj7xBaHMcN4lR24_fTIsAdwY-xkQ-RFWzw4-zDOag0M4xoCnVMQAvD_BwE)
   + Pull-based
     + **An agent watches the git repository for changes** and compares the definition in the repository with the actual running state. **If changes are detected, the agent applies the changes to the infrastructure.**
     + Tools: 
       + [Flux](https://fluxcd.io/)
         + Flux is built with the GitOps Toolkit
       + [ArgoCD](https://argo-cd.readthedocs.io/)
         + ArgoCD is implemented as a Kubernetes controller
         + ![](https://github.com/baozi929/Notes/blob/main/kubernetes/KCNA/figures/chap6_ArgoCDarchitecture.png)
           + Reference: https://argo-cd.readthedocs.io/en/stable/operator-manual/architecture/
4. Kubernetes
   + well suited for GitOps
     + why: it provides an API and is designed for declarative provisioning and changes of resources right from the beginning
   + Kubernetes is using a similar idea as the **pull-based approach**: A database is watched for changes and the changes are applied to the running state if it doesn’t match the desired state.
5. Learning GitOps + usage of ArgoCD and Flux
   + [Introduction To GitOps (LFS169)](https://training.linuxfoundation.org/training/introduction-to-gitops-lfs169/)



### Additional Resources

1. 10 Deploys Per Day - Start of the DevOps movement at Flickr

   - [Velocity 09: John Allspaw and Paul Hammond, "10+ Deploys Per Day"](https://www.youtube.com/watch?v=LdOe18KhtT4)

   - [10+ Deploys Per Day: Dev and Ops Cooperation at Flickr](https://www.slideshare.net/jallspaw/10-deploys-per-day-dev-and-ops-cooperation-at-flickr), by John Allspaw and Paul Hammond

2. Learn git in a playful way

   - [Oh My Git! An open source game about learning Git!](https://ohmygit.org/)

   - [Learn Git Branching](https://learngitbranching.js.org/)

3. Infrastructure as Code

   - [Delivering Cloud Native Infrastructure as Code](https://www.pulumi.com/why-pulumi/delivering-cloud-native-infrastructure-as-code/)

   - [Unlocking the Cloud Operating Model: Provisioning](https://www.hashicorp.com/resources/unlocking-the-cloud-operating-model-provisioning)

4. Beginners guide to CI/CD
   - [GitLab's guide to CI/CD for beginners](https://about.gitlab.com/blog/2020/07/06/beginner-guide-ci-cd/)
