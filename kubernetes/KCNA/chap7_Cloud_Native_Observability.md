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

1. Observability

   + What is observability?
     + observability is closely related to the **control theory** which deals with behavior of dynamic systems
     + the control theory describes **how external outputs of systems can be measured to manipulate the behavior of the system**
   + Example:
     + **In IT systems**: **autoscaling**
       + **set the desired utilization of the system and trigger scaling events** based on the load of the system
   + Challenges in container orchestration and microservices:
     + keeping track of the systems
     + how they interact with each other
     + how they behave when under load or in an error state
   + Observability should give answer to questions like:
     + Is the system stable or does it change its state when manipulated?
     + Is the system sensitive to change, e.g. if some services have high latency?
     + Do certain metrics in the system exceed their limits?
     + Why does a request to the system fail?
     + Are there any bottlenecks in the system?
   + The higher goal of observability is to allow **analysis** of the collected data ->
     + get a better understanding of the system
     + react to error states 

2. Telemetry

   + What telemetry mean?
     + **remote or distance** (tele) + **measuring** (metry)
   + Telemetry in system
     + measuring and collecting data points
     + transferring data to another system
   + Telemetry in container system
     + every application should have tools built in that **generate information data**, which is then **collected and transferred in a centralized system**
     + Categories of data:
       + Logs
         + errors, warnings or debug information
       + Metrics
         + quantitative measurements taken over time
         + example:
           + the number of requests
           + an error rate
       + Traces
         + track the progression of a request while it’s passing through the system
   + Traditional system VS Distributed system
     + Traditional system:
       + might not transmit data to a centralized system -> to view logs, need to connect to the system and read directly from files
     + Distributed system (might have hundreds or thousands of services):
       + very time consuming to troubleshoot with traditional ways

3. Logging

   + Easy to log to a file with different log levels (because application frameworks and programming languages come with **extensive logging tools built-in**)

   + Example:

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

       + docker: `$ docker logs nginx`

       + kubectl: 

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

   + Methods to ship logs to a system that stores the logs

     + **Node-level logging**
       + the most efficient way to collect logs
       + administrator **configures a log shipping tool** to collect logs and ship them to a central store
     + **Logging via sidecar container**
       + use **a sidecar container** to collect the logs and ship them to a central store
     + **Application-level logging**
       + the application pushes the logs directly to the central store
         + not as convenient as it may looks like, because it **requires configuring the logging adapter in every application that runs in a cluster**

   + Tools to ship and store the logs

     + Node-level logging and Logging via sidecar container can be done by:
       + [fluentd](https://www.fluentd.org/) or [filebeat](https://www.elastic.co/beats/filebeat)
     + Tools to store logs:
       + [OpenSearch](https://opensearch.org/) or [Grafana Loki](https://grafana.com/oss/loki/)

   + Log format

     + use a structured format like JSON to make logs easy to process and searchable
     + References:
       + [Structured logging (Google Cloud documentation)](https://cloud.google.com/logging/docs/structured-logging)
       + [Structured logging (Microsoft Azure documentation)](https://docs.microsoft.com/en-us/azure/architecture/example-scenario/logging/unified-logging#structured-logging)

4. Prometheus

   + [Prometheus](https://prometheus.io/) is an open source **monitoring system**
     + a [Cloud Native Computing Foundation](https://cncf.io/) graduated project
     + integrates well in the Kubernetes and container ecosystem
     
   + What Prometheus do?

     + Collect metrics (time series data), include **a timestamp, label and the measurement** itself
     + The **Prometheus data model** provides [four core metrics](https://prometheus.io/docs/concepts/metric_types/):
       - Counter: A value that increases, like a request or error count
       - Gauge: Values the increase or decrease, like memory size
       - Histogram: A sample of observations, like request duration or response size
       - Summary: Similar to a histogram, but also provides the total count of observations.

   + How to expose metrics?

     + applications can expose an HTTP endpoint under **/metrics**

     + Existing client libraries:
       + Go
       + Java or Scala
       + Python
       + Ruby
     + Client libraries: https://prometheus.io/docs/instrumenting/clientlibs/

5. Tracing

   + What is trace?
     + help to understand **how a request is processed** in a microservice architecture
     + A trace describes the tracking of a request while it passes through the services
       + consists of multiple units of work, which represent the different events that occur while the request is passing the system
       + Each application can contribute a span to the trace, which can include information like:
         + start and finish time
         + name
         + tags
         + a log message
   + Tracing system to store and analyze traces:
     + [Jaeger](https://www.jaegertracing.io/)
   + Standardization
     + [OpenTracing](https://opentracing.io/) + [OpenCensus](https://opencensus.io/) projects -> [OpenTelemetry](https://opentelemetry.io/) project (CNCF project)
       + a set of **application programming interfaces (APIs)**, **software development kits (SDKs)** and **tools** that can be used to **integrate telemetry**, such as
         + metrics
         + protocols
         + especially traces into applications and infrastructures
     + The OpenTelemetry clients can be used to **export telemetry data in a standardized format to central platforms** like Jaeger. 
       + Existing tools can be found in: [OpenTelemetry documentation](https://opentelemetry.io/docs/)

6. Cost Management

   + Key to cost optimization
     + analyze what is really needed
     + automate the scheduling of the resources needed
   + **Automatic and Manual Optimizations**
     + Identify wasted and unused resources
       + Monitor resource usage
         + -> find unused resources or servers that don't have a lot of idle time
       + Autoscaling
         + -> can help to shut down instance that are not needed
     + Right-Sizing
       + choose servers and systems with a lot more power than actually needed
       + good monitoring can give indications of how much resources are needed for application
     + Reserved Instances VS On-demand Instances
       + Use on-demand pricing models when need resources on-demand
       + A method to save a lot of money is to reserve resources and even pay for them upfront
     + Spot Instances
       + Use spot instances to save money when you have **a batch job or heavy load for a short amount of time**
       + idea of spot instances
         + get unused resources that have been over-provisioned by the cloud vender for very low price
       + problem
         + resources are not reserved for you -> can be terminated on short notice (to be used by someone else paying "full price")
   + Can use mixed solution to be more cost efficient
     + example: mix on-demand, reserved and spot instances

