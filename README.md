## Kickoff - Simple and efficient task scheduler for "heterogeneous" compute clusters

A few nice features Kickoff provides:

* Fully generic, cross platform, and language/framework-agnostic task scheduler
* Extremely efficient (written in pure C++) and scales easily to millions of tasks.
* Simple setup: Launch a server and add workers. Kickoff requires NO configuration.
* Heterogeneous: Each task and worker specifies which "affinities" it needs/supports.

For more detail and documentation, simply run "kickoff" after building, and read the help screen.

### NOTE

Kickoff is a work in progress project and hasn't been tested extensively. Also, it works on Windows, 
and Linux support is planned (and should be fairly simple) but not finished right now.

### Examples

To start a task server, simply run this command:

`kickoff server`

Now workers can connect to the server and start recieving tasks. Starting a worker process is as simple as:

`kickoff worker -affinity cpu gpu -server <server ip address>`

Note the "affinity" option; this specifies which requirement configurations the worker support. Adding a task to be
executed looks like this:

`kickoff new dostuff.py arg1 arg2 arg3 -affinity gpu -server <server ip address>`

The task here basically specifies that it needs workers which support the "gpu" affinity, and will therefore only be
given to workers that report support for that affinity.

Listing tasks currently waiting or being executed can be done via:

`kickoff status -server <server ip address>`

You can request runtime status info about a particular task via:

`kickoff info <task id> -server <server ip address>`

And finally, you may choose to cancel tasks via:

`kickoff cancel <task id> -server <server ip address>`
