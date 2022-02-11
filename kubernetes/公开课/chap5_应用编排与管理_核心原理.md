# 一、资源元信息

### 1. Kubernetes 资源对象

+ Spec：期望的状态
+ Status：观测到的状态
+ Metadata
  + Labels：用来识别资源
  + Annotations：用于描述资源
  + OwnerReference：用来描述多个资源之间相互关系

### 2. Labels

+ 标识性的key-value元数据

  + 例子：

    + ```
      environment: production
      release: stable
      app.kubernetes.io/version: 5.7.21
      failure-domain.beta.kubernetes.io/region: cn-hangzhou
      ```

+ 作用

  + 筛选资源
  + 唯一的组合资源的方法

+ 可以使用selector来查询

### 3. Selector

+ 相等型selector
  + 可包含多个相等条件（多个条件之间是逻辑与的关系）
  + 例：Tie=front,Env=dev
+ 集合型selector
  + Env in (test, gray)
  + tie notin (front, back)
  + 是否带有某些标签：
    + release
    + !release

### 4. Annotations

+ 作用：
  + 存储资源的非标识性信息
  + 扩展资源的 spec/status
+ 特点：
  + 一般比 label 更大
  + 可以包含特殊字符
  + 可以结构化也可以非结构化

### 5. OwnerReference

+ “所有者”，即集合类资源
  + Pod 的集合：replicaset，statefulset
+ 集合类资源的控制器创建了归属资源
  + Replicaset 控制器创建 Pod
+ 作用
  + 方便反向查找创建资源的对象
  + 方便进行级联删除

# 二、操作演示

