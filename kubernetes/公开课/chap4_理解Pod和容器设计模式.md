# 一、为什么需要 Pod

### 容器的基本概念

1. 容器的本质：
   - 视图被隔离、资源受限的进程
     - 容器中 PID = 1 的进程即为应用本身
     - 管理虚拟机 =  管理基础设施；管理容器 =  管理应用本身
2. Kubernetes？
   - 云时代的操作系统
     - 容器镜像：可以理解为这个操作系统的软件安装包





# 二、Pod 的实现机制

### 真实操作系统的例子：helloworld

- helloworld 程序由一组进程（linux中的线程）组成：

  - ```
    $ pstree -p
    ...
      |-helloworld,3062
      |   |-{api},3063
      |   |-{main},3064
      |   |-{log},3065
      |   `|-{compute},3133
    ```

  - 这 4 个进程共享 helloworld 程序的资源，互相协作，完成 helloworld 程序的工作

- 类比：
  - Kubernetes = 操作系统（如：Linux）
  - 容器 =  进程（linux 线程）
  - Pod = 进程组（Linux 线程组）

### “进程组”概念

+ 例子：
  + helloworld 程序，由 4 个进程组成，这些进程共享某些文件
+ 问题：如何将 helloworld 程序用容器跑起来？
  + 解法1：再一个 Docker 容器中，启动 4 个进程
    + 问题：容器 PID = 1 的进程是应用本身，比如 main 进程，那么谁来负责管理剩余的 3 个进程？
      + **容器是单进程模型**
        + 如果你在容器里启动多个进程，只有一个可以作为 PID=1 的进程；如果 PID = 1 的进程挂了、或失败退出，其他进程没有人能够管理、回收资源
      + 存在问题：
        + 应用本身不具备**进程管理**能力
          + 如果 PID = 1 进程为应用本身，并且该进程在运行过程中被 kill 了或者自己死掉了，剩下 3 个进程的资源没有人回收！！
        + 如果将容器的 PID = 1 进程改成 systemd
          + 管理容器 = 管理 systemd != 管理应用本身
          + 应用状态的生命周期不等于容器的生命周期，增加了管理的复杂性
            + 如：容器中的应用是否退出、是否出现异常等，无法直接知道
  + 解法2：Kubernetes 中，Pod = "进程组"；可将 helloworld 程序定义为拥有 4 个容器的 Pod
    + ![](https://edu.aliyun.com/files/course/2021/04-02/15520008a554237835.png)
    + Kubernetes 启动后，会有 4 个容器。这 4 个容器共享某些资源，这些资源都属于 Pod
    + Pod 是 Kubernetes 分配资源的一个单位，因为里面的容器要共享某些资源，所以 Pod 也是 Kubernetes 的**原子调度单位**

### 为什么 Pod 必须是原子调度单位？

1. 举例：
   - 描述：两个容器紧密协作
     - App：业务容器，写日志文件
     - LogCollector：转发日志文件到 ElasticSearch 中
   - 内存要求：
     - App：1G
     - LogCollector：0.5G
   - 当前可用内存：
     - Node_A：1.25G
     - Node_B：2G
   - Task co-scheduling 问题:
     - 例子：
       - 假设没有 Pod 概念，只有 2 个需要紧密协作、运行在一台机器上的容器
       - 如果调度器先把 App 调度到 Node_A 上，则 LogCollector 无法调度到 Node_A 上，调度失败，需要重新调度
     - 解法：
       - Mesos：资源囤积（resource hoarding）
         - 所有设置了 Affinity 约束的任务都打到时，才开始统一进行调度
         - 问题：调度效率损失和死锁
       - Google Omega（Borg的下一代）：乐观调度处理冲突
         - 不管这些冲突的异常情况，先调度，同时设置一个非常精妙的回滚机制，这样经过冲突后，通过回滚来解决问题
         - 问题：实现机制较为复杂
       - Kubernetes：Pod
         - 在 Kubernetes 里，App 容器和 LogCollector 容器属于一个 Pod 的，它们在调度时必然是以一个 Pod 为单位进行调度，所以这个问题是根本不存在的

### 再次理解 Pod

+ 亲密关系 - 调度解决
  + 两个应用需要运行在同一台宿主机上
+ 超亲密关系 - Pod 解决
  + 会发生直接的文件交换
  + 使用 localhost 或者 Socket 文件进行本地通信
  + 会发生非常频繁的 RPC 调用
  + 会共享某些 Linux Namespace
    + 例：一个容器要加入另一个容器的Network Namespace（从而可以看到另一个容器的网络设备和网络信息）
  + ...



# 二、Pod 的实现机制

### Pod 要解决的问题

+ 如何让一个 Pod 中的多个容器之间最高效地共享某些数据和资源？
  + 容器之间原本是被 Linux Namespace 和 cgroups 隔开的 -> 打破隔离，共享信息
+ 共享网络
  + 例：有一个 Pod，包含容器 A 和容器 B，现在要共享 Network Namespace
  + 解法：
    + 每一个 Pod 中额外起一个 Infra container 小容器来共享整个 Pod 的 Network Namespace
      + Infra container：小镜像，100~200KB，永远处于“暂停”状态的容器
      + 有这个镜像之后，其他所有容器会通过 Join Namespace 的方式加入到 Infra container 的 Network Namespace 中
      + Pod 中所有容器看到的网络视图相同（即网络设备、IP地址、Mac地址等等），可以直接使用 localhost 进行通信
    + 一个 Pod 只有一个 IP 地址（即这个 Pod 的 Network Namespace 对应的 IP 地址）
    + Pod 的生命周期和 Infra container一致，而与容器 A 和 B 无关
+ 共享存储
  + ![](https://edu.aliyun.com/files/course/2021/04-02/1553553c8f06086674.png)
  + volumn 变成 Pod level，Pod 中所有容器共享所有的 volumn
  + 上图中的例子：
    +  volumn 叫做 shared-data，属于 Pod level
    + 容器中可以声明要挂载 shared-data 这个 volumn，只要声明了挂载这个 volumn，就可以在容器内看到这个目录





# 三、详解容器设计模式

### 举例：WAR 包 + Tomcat 的容器化

- 方法1：把 WAR 包和 Tomcat 打包放进一个镜像里面
  - 问题：无论更新 WAR 包还是 Tomcat，都需要重新制作镜像
- 方法2：镜像里面只打包 Tomcat，使用数据卷从宿主机上将 WAR 包挂载进 Tomcat 容器
  - 问题：需要维护一套分布式存储系统
- 方法3：Kubernetes - InitContainer

### InitContainer

+ ![](https://edu.aliyun.com/files/course/2021/04-02/15545603d4db884440.png)
+ 说明：
  + InitContainer 比 spec.container 定义的用户容器先启动，并且严格按照定义顺序依次启动
    + 在上图的 yaml 里，首先定义一个 InitContainer，它只做一件事情，就是把 WAR 包从镜像里拷贝到一个 Volume 里面，它做完这个操作就退出了
  + 拷贝到的目录 /app 目录是一个 Volumn （Pod 中的多个容器可以共享该 Volumn）
  + Tomcat 容器，只打包了一个 Tomcat 镜像；但在启动时，通过声明使用该 Volumn 作为容器的 Volumn，并挂载到 webapps 目录下
    + 当 Tomcat 启动时，它的 webapps 目录下就会存在 sample.war

### 容器设计模式：Sidecar

+ 在 Pod 里面，可以定义一些专门的容器，来执行主业务容器所需要的一些辅助工作

+ 例：

  + 上面的 InitContainer，它就是一个 Sidecar，它只负责把镜像里的 WAR 包拷贝到共享目录里面，以便被 Tomcat 能够用起来

+ 其他操作

  + 原本需要 SSH 进去执行的脚本

  - 日志收集

  - Debug 应用

  - 应用监控
  - ...

+ 优势：

  + 将辅助功能从业务容器**解耦**了，能够独立发布 Sidecar 容器，并且更重要的是这个能力是**可以重用**的

+ 具体场景例：

  + ![](https://edu.aliyun.com/files/course/2021/04-02/1556055d8711932255.png)
  + ![](https://edu.aliyun.com/files/course/2021/04-02/15563319cfe4124285.png)
  + ![](https://edu.aliyun.com/files/course/2021/04-02/15570404484d628788.png)



# 本节总结

- Pod 是 Kubernetes 项目里实现“容器设计模式”的核心机制；
- “容器设计模式”是 Google Borg 的大规模容器集群管理最佳实践之一，也是 Kubernetes 进行复杂应用编排的基础依赖之一；
- 所有“设计模式”的本质都是：解耦和重用。