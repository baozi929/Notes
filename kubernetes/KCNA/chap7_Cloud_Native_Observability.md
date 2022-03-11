# Introduction

1. Chapter Overview
   + Conventional monitoring for servers
     + collecting basic metrics of the system like CPU and memory resource usage
     + logging of processes and the operating system
     + ....
   + Challenge for a microservice architecture: **monitoring requests that move through a distributed system**
2. Learning Objectives
   + Explain why **observability** is **a key discipline of cloud computing**.
   + Discuss metrics, logs and traces.
   + Understand how to **show logs of containerized applications**.
   + Explain how **Prometheus** can be used to **collect and store metrics**.
   + Understand how to **optimize cloud costs**.



# Cloud Native Observability

### Observability

1. Observability VS Monitoring

   + Monitoring: One of the subcategories of cloud native observability

   + What is **observability**?
     + observability is closely related to the **[control theory](https://en.wikipedia.org/wiki/Control_theory)** which deals with behavior of dynamic systems
       + the control theory describes **how external outputs of systems can be measured to manipulate the behavior of the system**

2. Examples:

   + The cruise control system of a car:
     + Set the desired speed (can be measured and observed) -> to maintain the speed, the power of the motor should be adapted

   + **In IT systems**: **autoscaling**
     + **set the desired utilization of the system and trigger scaling events** based on the load of the system

3. **Challenges** in container orchestration and microservices:

   + **keeping track of the systems**

   + how they **interact** with each other

   + how they behave when **under load or in an error state**

4. Observability should give answer to questions like:

   + Is the system stable or does it change its state when manipulated?

   + Is the system sensitive to change, e.g. if some services have high latency?

   + Do certain metrics in the system exceed their limits?

   + Why does a request to the system fail?

   + Are there any bottlenecks in the system?

5. The higher goal of observability is to allow **analysis** of the collected data ->

   + get a better understanding of the system

   + react to error states



### Telemetry

1. What telemetry mean?
   + **remote or distance** (tele) + **measuring** (metry)

2. Telemetry in system

   + **measuring and collecting data points**

   + **transferring data** to another system

3. Telemetry in container system

   + every application should have tools built in that
     + **generate information data**
     + then **collected and transferred in a centralized system**

   + Categories of data:
     + Logs
       + errors, warnings or debug information
     + Metrics
       + **quantitative measurements** taken over time
       + example:
         + the number of requests
         + an error rate
     + Traces
       + track **the progression of a request** while it’s passing through the system

4. Traditional system VS Distributed system

   + Traditional system:
     + **might not transmit data to a centralized system** -> to view logs, need to connect to the system and read directly from files

   + Distributed system (might have hundreds or thousands of services):
     + very time consuming to troubleshoot with traditional ways



### Logging

1. Easy to log to a file with different log levels (because application frameworks and programming languages come with **extensive logging tools built-in**)

2. Example for logs :

   + python: https://docs.python.org/3/howto/logging.html#logging-to-a-file


   + Unix and linux programs provide 3 I/O streams:

     + standard input (stdin):
       + Input to a program (e.g. via keyboard)
     + standard output (stdout):
       + The output a program writes on the screen 
     + standard error (stderr):
       + Errors that a program writes on the screen 
     + Reference: https://man7.org/linux/man-pages/man3/stdout.3.html


   + Command line tools like **docker, kubectl or podman** provide command to show logs of containerized processes

     + docker: 

       + example: view the logs of a container named **nginx**
         + `$ docker logs nginx`
     
     + kubectl logs:
     
       + format:
     
         + `$ kubectl logs [-f] [-p] (POD | TYPE/NAME) [-c CONTAINER]`
     
       + parameters:
     
         + -f (follow): Stream the logs **in real time**
         + -c (container): Print the logs of this container
         + -p (previous): Print the logs for **the previous instance of the container in a pod** if it exists
         + since: Only return logs **newer than a relative duration** like 5s, 2m, or 3h
         + tail: Lines of **recent** log file to display.
     
       + Examples:

         + ```
           # Return snapshot logs from pod nginx with only one container
           kubectl logs nginx 
           
           # Return snapshot of previous terminated ruby container logs from pod web-1
           kubectl logs -p -c ruby web-1 
           
           # Begin streaming the logs of the ruby container in pod web-1
           kubectl logs -f -c ruby web-1 
           
           # Display only the most recent 20 lines of output in pod nginx
           kubectl logs --tail=20 nginx 
           
           # Show all logs from pod nginx written in the last hour
           kubectl logs --since=1h nginx
           ```
     
       + Reference: https://kubernetes.io/docs/reference/generated/kubectl/kubectl-commands#logs

3. Methods to ship logs to a system that stores the logs

   + **Node-level logging**
     + the most efficient way to collect logs
     + administrator **configures a log shipping tool** to collect logs and ship them to a central store

   + **Logging via sidecar container**
     + use **a sidecar container** to collect the logs and ship them to a central store

   + **Application-level logging**
     + the application pushes the logs directly to the central store
       + not as convenient as it may looks like, because it **requires configuring the logging adapter in every application that runs in a cluster**

4. Tools to ship and store the logs

   + Node-level logging and Logging via sidecar container can be done by:
     + [fluentd](https://www.fluentd.org/) or [filebeat](https://www.elastic.co/beats/filebeat)

   + Tools to store logs:
     + [OpenSearch](https://opensearch.org/) or [Grafana Loki](https://grafana.com/oss/loki/)

5. Log format

   + use a structured format like JSON to make logs easy to process and searchable

   + References:
     + [Structured logging (Google Cloud documentation)](https://cloud.google.com/logging/docs/structured-logging)
     + [Structured logging (Microsoft Azure documentation)](https://docs.microsoft.com/en-us/azure/architecture/example-scenario/logging/unified-logging#structured-logging)



### Prometheus

1. [Prometheus](https://prometheus.io/) is an open source **monitoring system**
   + 2nd CNCF hosted project, a [Cloud Native Computing Foundation](https://cncf.io/) graduated project
   + popular monitoring solution, now is **a standard tool that integrates well in the Kubernetes and container ecosystem**

2. What can Prometheus do?

   + Collect metrics 
     + **time series data**, include **a timestamp, label and the measurement** itself

   + The **Prometheus data model** provides [four core metrics](https://prometheus.io/docs/concepts/metric_types/):
     - Counter:
       - A **cumulative metric**
       - Value can only **increases**
       - Example: a request or error count
     - Gauge:
       - A metric that represents a single numerical value
       - Values can **increase or decrease**
       - Example: memory size
     - Histogram:
       - A sample of observations, **counts observations in configurable buckets**
       - Example: request duration or response size
     - Summary:
       - Similar to a histogram, but also **provides the total count of observations**

3. How to expose metrics?

   + applications can **expose an HTTP endpoint** under **/metrics**

   + Existing client libraries:
     + Go
     + Java or Scala
     + Python
     + Ruby

   + Client libraries: https://prometheus.io/docs/instrumenting/clientlibs/

4. Example of exposed data

   + ```
     # HELP queue_length The number of items in the queue.
     # TYPE queue_length
     gauge queue_length 42
     # HELP http_requests_total The total number of handled HTTP requests.
     # TYPE http_requests_total counter
     http_requests_total 7734
     # HELP http_request_duration_seconds A histogram of the HTTP request durations in seconds.
     # TYPE http_request_duration_seconds histogram http_request_duration_seconds_bucket{le="0.05"} 4599
     http_request_duration_seconds_sum 88364.234
     http_request_duration_seconds_count 227420
     # HELP http_request_duration_seconds A summary of the HTTP request durations in seconds.
     # TYPE http_request_duration_seconds summary
     http_request_duration_seconds{quantile="0.5"} 0.052
     http_request_duration_seconds_sum 88364.234
     http_request_duration_seconds_count 227420
     ```

5. Prometheus in Kubernetes

   + build-in support for Kubernetes

   + can be configured **automatically discover all services** in the cluster and collect the metric data in defined interval -> save data in a **time series database**

   + How to **query data** from the time series database？

     + query language: [PromQL (Prometheus Query Language)](https://prometheus.io/docs/prometheus/latest/querying/basics/)

     + can be used to:

       + select and aggregate data in real time and view data in the built-in Prometheus UI

     + Examples:

       + References: https://prometheus.io/docs/prometheus/latest/querying/examples/

       + Return time series metric:

         + ```
           # Return all time series with the metric http_requests_total and the given job and handler labels:
           http_requests_total{job="apiserver", handler="/api/comments"}
           ```

       + Use functions:

         + ```
           # Return the per-second rate for all time series with the http_requests_total metric name, as measured over the last 5 minutes:
           rate(http_requests_total[5m])
           ```

         + these functions can be used to get an indication on how a certain value increase or decreases over time

           + -> help analyze errors or predicting failures for an application

6. Tools in Prometheus ecosystem

   1. **Grafana**

      + Reference: https://grafana.com/grafana/
      + most used companion for Prometheus
      + What can Grafana do?
        + build dashboards from collected metrics

   2. **Alertmanager**

      + Reference: https://prometheus.io/docs/alerting/latest/alertmanager/

      + **When the alert is firing**, Alertmanager can **send a notification** out to your favorite persistent chat tool, e-mail or specialized tools that are made for alerting and on-call management

      + Example for an alerting rule in Prometheus:

        + ```
          groups:
          - name: example
            rules:
            - alert: HighRequestLatency
              expr: job:request_latency_seconds:mean5m{job="myjob"} > 0.5 for: 10m
              labels:
                severity: page
              annotations:
                summary: High request latency
          ```



### Tracing

1. What is trace?

   + help to understand **how a request is processed** in a microservice architecture

   + A trace describes the tracking of a request while it passes through the services
     + consists of multiple units of work, which **represent the different events** that occur while the request is passing the system
     + **Each application can contribute a span to the trace**, which can include information like:
       + start and finish time
       + name
       + tags
       + a log message

2. Tracing system to store and analyze traces:
   + [Jaeger](https://www.jaegertracing.io/)

3. Standardization

   + [OpenTracing](https://opentracing.io/) + [OpenCensus](https://opencensus.io/) projects -> [OpenTelemetry](https://opentelemetry.io/) project (CNCF project)
     + a set of **application programming interfaces (APIs)**, **software development kits (SDKs)** and **tools** that can be used to **integrate telemetry**, such as
       + metrics
       + protocols
       + especially traces into applications and infrastructures

   + The OpenTelemetry clients can be used to **export telemetry data in a standardized format to central platforms** like [Jaeger](https://www.jaegertracing.io/). 
     + Existing tools can be found in: [OpenTelemetry documentation](https://opentelemetry.io/docs/)



### Cost Management

1. Key to cost optimization

   + analyze what is really needed

   + automate the scheduling of the resources needed

2. **Automatic and Manual Optimizations**

   + **Identify wasted and unused resources**
     + Monitor resource usage
       + -> find unused resources or servers that don't have a lot of idle time
     + Autoscaling
       + -> can help to shut down instance that are not needed

   + **Right-Sizing**
     + At the start, choose servers and systems with a lot more power than actually needed
     + **good monitoring can give indications of how much resources are needed for application**

   + Reserved Instances VS On-demand Instances
     + Use on-demand pricing models when need resources on-demand
     + A method to save a lot of money is to **reserve resources and even pay for them upfront**

   + Spot Instances
     + Use spot instances to save money when you have **a batch job or heavy load for a short amount of time**
     + idea of spot instances
       + get **unused resources** that have been **over-provisioned by the cloud vender** for very low price
     + problem
       + resources are not reserved for you -> can be terminated on short notice (to be used by someone else paying "full price")

3. Can use mixed solution to be more cost efficient
   + example: mix on-demand, reserved and spot instances



### Additional Resources

1. Cloud Native Observability
   - [The Cloud Native Landscape: Observability and Analysis](https://thenewstack.io/the-cloud-native-landscape-observability-and-analysis/)

2. Prometheus
   - [Prometheus Cheat Sheet - Basics (Metrics, Labels, Time Series, Scraping)](https://iximiuz.com/en/posts/prometheus-metrics-labels-time-series/), by Ivan Velichko (2021)

3. Prometheus at scale

   - [Thanos](https://thanos.io/)

   - [Cortex](https://cortexmetrics.io/)

4. Logging for Containers
   - [Use the native logging mechanisms of containers](https://cloud.google.com/architecture/best-practices-for-operating-containers#use_the_native_logging_mechanisms_of_containers) (Google Cloud)

5. Right-Sizing and cost optimization

   - [Right Sizing](https://aws.amazon.com/aws-cost-management/aws-cost-optimization/right-sizing/) (Amazon AWS)

   - [Cloud cost optimization: principles for lasting success](https://cloud.google.com/blog/topics/cost-management/principles-of-cloud-cost-optimization) (Google Cloud)
