## Kickoff - Simple and efficient task scheduler for "heterogeneous" compute clusters

A few nice features Kickoff provides:

* Fully generic and language/framework-agnostic task scheduler
* Extremely efficient (written in pure C++) and scales easily to millions of tasks.
* Simple setup: Launch a server and add workers. Kickoff requires NO configuration.
* Heterogeneous: Each task specifies which "resource tags" it needs and/or prefers.
* Flexible: Each worker may dynamically update which resources it has.

For more detail and documentation, simply run "kickoff" after building, and read the help screen.

### NOTE

Kickoff is a work in progress project and hasn't been tested extensively. Also, it works on Windows, 
and Linux support is planned (and should be fairly simple) but not implemented yet.

### Examples

To start a task server, simply run this command:

`kickoff server`

Now workers can connect to the server and start recieving tasks. Starting a worker process is as simple as:

`kickoff worker -have cpu gpu -server <server ip address>`

Note the "affinity" option; this specifies which requirement configurations the worker support. Adding a task to be
executed looks like this:

`kickoff new <command to execute> -require cpu -want gpu -server my_task_server`

The task here basically specifies that it needs workers which claim they have the "cpu" resource tag, but also prefers
that they have the "gpu" resource tag if at all possible. Note that this is fully generic -- Kickoff itself has no
concept of what "gpu" or "cpu" tags mean; it simply tracks their availability across workers and matches tasks as appropriate.

Listing tasks currently waiting or being executed can be done via:

`kickoff status -server <server address>`

You can request runtime status info about a particular task via:

`kickoff info <task id> -server <server address>`

And finally, you may choose to cancel tasks via:

`kickoff cancel <task id> -server <server address>`
