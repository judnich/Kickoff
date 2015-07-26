## Kickoff - Efficient task execution graph system for "heterogeneous" distributed compute clusters

A few nice features Kickoff provides:

* Fully generic, cross platform, and language/framework-agnostic task dispatcher
* Extremely efficient (written in pure C++) and scales easily to millions of tasks.
* Simple setup: Launch a server and add workers. Kickoff requires NO configuration.
* Supports arbitrary dependency graphs (e.g. not just map-reduce).
* Heterogeneous: Each task and worker specifies which "affinities" it needs/supports.

Kickoff works great for a wide range of workflows, ranging from massively distributed computing with map-reduce style
algorithm, to simple and quick task queues, or "cloud make" flows.

For more detail and documentation, simply run "kickoff" after building, and read the help screen.

### NOTE

Kickoff is a work in progress project and hasn't been tested extensively. It's functional and very fast but 
additional work is being done to make it even more efficient. Also, it works on Windows, and Linux support 
is planned (and should be fairly simple) but not finished right now.

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

Not shown here is Kickoff's ability to handle arbitrary dependency graphs. This simply means that any task may
give a list of tasks that must complete before it's allowed to run. In such cases, cancel has a slightly different
behavior in that canceling a task will not only cancel that task alone but all tasks that depend on it.
