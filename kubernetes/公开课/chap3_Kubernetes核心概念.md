# 什么是 Kubernetes

+ 工业级的容器编排平台

  + 自动化的容器编排平台
  + 负责应用的部署、弹性、管理
  + 基于容器

+ Kubernetes 的核心功能

  + 服务的发现与负载的均衡

  - 容器的自动装箱
  - 存储的编排

  - 自动化的容器的恢复

  - 应用的自动发布与应用的回滚，以及与应用相关的配置密文的管理

  - 对于 job 类型任务，Kubernetes 可以去做批量的执行

  - 为了让这个集群、这个应用更富有弹性，Kubernetes 也支持水平的伸缩

+ 例子：

  + 调度
    + 容器可以被放置到集群的某一个节点（根据容器所需 CPU、memory 等，将其放置到较为空闲的机器）
  + 自动恢复
    + Kubernetes 具有节点健康检查功能。Kubernetes 会将失败节点上的容器自动迁移到健康运行的宿主机，以实现集群内容器的自动恢复
  + 水平伸缩
    + Kubernetes 具有业务负载检查能力。如果某个业务 CPU 利用率过高，或相应时间过长，Kubernetes 可对该业务进行扩容



# Kubernetes 的架构

+ 二层 架构和server-client 的架构

  + ![](https://edu.aliyun.com/files/course/2021/04-02/153434ac4fc5287155.png)
  + Master 为中央管控节点，与 Node 进行连接
  + User 侧的组件，如 UI、Clients等，只与 Master 进行连接，把希望的状态或想执行的命令下发给 Master，Master 将这些命令或者状态下发给相应的节点，进行最终的执行

+ Master的组件

  + ![](https://edu.aliyun.com/files/course/2021/04-02/153518638ffc816504.png)
  + **API Server：**
    + 用来处理 API 操作。Kubernetes 中所有的组件都会和 API Server 进行连接，组件与组件之间一般不进行独立的连接，都依赖于 API Server 进行消息的传送
    + 可水平扩展的部署组件

  - **Controller：**
    - 控制器，用来完成对集群状态的管理。例如，自动对容器进行修复、自动进行水平扩张等，都是由 Kubernetes 中的 Controller 来进行完成的
    - 可进行热备的部署组件，只有一个active

  - **Scheduler：**调度器，完成调度的操作。例如，把一个用户提交的 Container，依据它对 CPU、对 memory 请求大小，找一台合适的节点，进行放置
    - 可进行热备的部署组件，只有一个active

  - **etcd：**一个分布式的一个存储系统，API Server 中所需要的这些原信息都被放置在 etcd 中，etcd 本身是一个高可用系统，通过 etcd 保证整个 Kubernetes 的 Master 组件的高可用性

+ **Kubernetes 的架构：Node**

  + ![](https://edu.aliyun.com/files/course/2021/04-02/153559f9addd084872.png)
    + Node
      + Kubernetes 的 Node 是真正运行业务负载的，每个业务负载会以 Pod 的形式运行
    + Pod
      + 一个 pod 中运行着一个或者多个容器，运行 pod 的组件为 kubelet
  + ![](https://edu.aliyun.com/files/course/2021/04-02/1536342d14bf291564.png)
    1. 用户通过 UI 或 CLI 提交 Pod 给 Kubernetes 进行部署
    2. 该请求通过 CLI 或 UI 提交给 Kubernetes 的 API Server
    3. API Server 将该信息写入其存储系统 etcd
    4. Scheduler 通过 API Server 的 watch（notification 机制）得到该信息：有一个 Pod 需要被调度
    5. Scheduler 根据内存状况进行调度决策，并在完成调度之后，向 API Server 报告这个Pod 需要被调度到某个节点
    6. API Server 接收到信息，再次将结果写入 etcd，并通知相应的节点进行这次 Pod 的真正启动
    7. 对应节点的 kubelet 会得到通知，调 Container runtime 去启动配置这个容器和这个容器的运行环境，调 Storage Plugin 去配置内存，network Plugin 去配置网络



# Kubernetes 的核心概念与它的 API

#### 第一个概念：Pod

+ 最小的调度以及资源单元
+ 由一个或多个容器组成
+ 定义容器的运行方式（Command、环境变量等）
+ 提供给容器共享的运行环境（网络、进程空间）
  + pod 内的容器可以通过 localhost 进行连接；pod 与 pod 之间存在隔离



#### 第二个概念：Volumn

+ 声明在 Pod 中的容器可访问的文件目录
+ 可以被挂载在 Pod 中一个或多个容器的指定路径下
+ 支持多种后端存储的抽象
  + 本地存储、分布式存储、云存储.......



#### 第三个概念：Deployment

+ Pod 上层的抽象，定义一组 Pod 的副本数目、版本等
  + 一般用 Deployment 的抽象做应用的管理
  + Pod 是组成 Deployment 最小的单元
+ 通过控制器（Controller）维持 Pod 的数目
  + 自动恢复失败的 Pod
+ 通过控制器以制定的策略控制版本
  + 滚动升级、重新生成、回滚等



#### 第四个概念：Service

+ 提供访问一个或多个 Pod 实例的稳定访问地址
  + 目的：提供固定的虚拟 IP 地址，而不是具体 Pod 的 IP 地址（因为 Pod 失败可能换成新的 Pod，但外部用户不会希望不停更换 Pod 地址）
+ 支持多种访问方式的实现
  + ClusterIP
  + NodePort
  + LoadBalancer



#### 第五个概念：Namespaces

+ 一个集群内部的逻辑隔离机制（鉴权、资源额度）
+ 每个资源都属于一个 Namespace
+ 同一个 Namespace 中的资源命名唯一；不同 Namespace 中的资源可以重名



#### Kubernetes 的 API 的基础知识

+ Kubernetes API 是由 **HTTP+JSON/YAML** 组成的：用户访问的方式是 HTTP；访问的 API 中 content 的内容是 JSON 格式的
  + kubectl
  + UI
  + curl
+ API - Label
  + 一组 Key:Value
  + 可以被 selector 所查询
    + select color=red
  + 资源集合的默认表达形式
    + 例如 Service 对应的一组 Pod