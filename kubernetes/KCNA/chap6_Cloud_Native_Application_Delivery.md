# Introduction

1. Chapter Overview
   + Change of methods to deploy an application on different platforms
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

1. Application Delivery Fundamentals

   + Steps to deliver applications

     + Step1: Source Code
       + The best way to manage source code: **Version control**
         + Version control tool: Git

     + Step2: Build source code
       + could also include the building of a container image

     + Step3: Automation test of the app to make sure all functionality still works fine after code change

     + Step4: Deliver the application to the platform it should run on
       + If target platform is Kubernetes, need YAML file to deploy the app -> build container image and push to a container registry

   + **Source code is not the only thing managed in a version control system**

     + **Infrastructure as Code** (IaC): https://en.wikipedia.org/wiki/Infrastructure_as_code

2. CI/CD

   + Requirements:
     + service get smaller
     + deployments get more frequent
   + Solution:
     + **Automation of the deployment process**
       + use the principles of **Continuous Integration/Continuous Delivery (CI/CD)**, which describe the different steps in the deployment of an application, configuration or even infrastructure
       + Part1: Continuous Integration
         + includes permanent building and testing of the written code
       + Part2: Continuous Delivery
         + automates the deployment of the pre-built software
       + -> To automate the whole workflow -> **CI/CD pipeline**
         + the scripted form of all the steps involved, running on a server or even in a container
   + Popular CI/CD tools:
     + [Spinnaker](https://spinnaker.io/)
     + [GitLab](https://gitlab.com/#)
     + [Jenkins](https://www.jenkins.io/)
     + [Jenkins X](https://jenkins-x.io/)
     + [Tekton CD](https://github.com/tektoncd/pipeline)
     + [Argo](https://argoproj.github.io/)
   + [Introduction to DevOps and Site Reliability Engineering (LFS162)](https://training.linuxfoundation.org/training/introduction-to-devops)

3. GitOps

   + Infrastructure as Code
     + increasing the quality and speed of providing infrastructure
     + configuration, network, policies, or security can be described as code
   + 2 approaches how a CI/CD pipeline can implement the changes you want to make:
     + Push-based
       + The pipeline is started and runs tools that make the changes in the platform. **Changes can be triggered by a commit or merge request**.
     + Pull-based
       + **An agent watches the git repository for changes** and compares the definition in the repository with the actual running state. **If changes are detected, the agent applies the changes to the infrastructure.**
       + Tools: 
         + [Flux](https://fluxcd.io/)
           + Flux is built with the GitOps Toolkit
         + [ArgoCD](https://argo-cd.readthedocs.io/)
           + ArgoCD is implemented as a Kubernetes controller
   + Kubernetes
     + well suited for GitOps
       + provides an API and is designed for declarative provisioning and changes of resources right from the beginning
       + Kubernetes is using a similar idea as the **pull-based approach**: A database is watched for changes and the changes are applied to the running state if it doesn’t match the desired state.

