.. _using-ethernet:

==========================
Using Chapel with Ethernet
==========================

There are two ways to use Ethernet

* Using `CHPL_COMM=gasnet` with :ref:`UDP conduit <using-udp>`
* Using `CHPL_COMM=ofi` with the :ref:`tcp provider <using-ofi-tcp>`

.. _using-udp:

------------------------------
Using the Portable UDP Conduit
------------------------------

This document describes how to run Chapel across multiple machines in a
portable manner using UDP for communication. This strategy is appropriate
for machines only connected by Ethernet, for example. See also
:ref:`readme-multilocale` which describes general information about
running Chapel in a multilocale configuration. 

The *UDP conduit* is a portion of the GASNet networking library. See
:ref:`what-is-gasnet` for more information about GASNet.

.. note::

  While the UDP conduit can help get multilocale Chapel programs running
  in a variety of environments, it is not expected to achieve the best
  performance. It is implemented for portability rather than for maximum
  performance.

To build Chapel with the UDP conduit, run these commands from ``$CHPL_HOME``:

.. code-block:: bash

   export CHPL_COMM=gasnet
   export CHPL_COMM_SUBSTRATE=udp
   make

Now you should be able to compile a program:

.. code-block:: bash

   chpl -o hello6-taskpar-dist $CHPL_HOME/examples/hello6-taskpar-dist.chpl

But, in order to run this program, you'll need to indicate where the
multi-locale program should run.

First, try running it locally:

.. code-block:: bash

   # Run over-subscribed on this machine
   export GASNET_SPAWNFN=L
   ./hello6-taskpar-dist -nl 2

You should see output from 2 locales that both report the same hostname. This
configuration simulates multiple Chapel locales with one workstation. This
configuration is useful for testing but is not expected to perform well.

Next, try running it across several machines.

.. code-block:: bash

   # Use SSH to spawn jobs
   export GASNET_SPAWNFN=S
   # Which ssh command should be used? ssh is the default.
   export GASNET_SSH_CMD=ssh
   # Disable X11 forwarding
   export GASNET_SSH_OPTIONS=-x
   # Specify which hosts to spawn on; SSH_SERVERS can be used equivalently
   export GASNET_SSH_SERVERS="host1 host2 host3 ..."

where host1, host2, host3, ... are the names of the
workstations that will serve as your Chapel locales.  In
order to run your Chapel program on k locales, you must
have k entries in the ``GASNET_SSH_SERVERS`` variable.  To avoid
typing in passwords for each node, you will probably want
to use normal ssh-agent/ssh-add capabilities to support
password-less ssh-ing.

Now running

.. code-block:: bash

  ./hello6-taskpar-dist -nl 2

should display 2 different hostnames that you specified in GASNET_SSH_SERVERS.

GASNet's UDP conduit can be configured with many other options. Please refer
to:

   * ``$CHPL_HOME/third-party/gasnet/gasnet-src/udp-conduit/README``
   * https://gasnet.lbl.gov/dist/udp-conduit/README


.. _using-udp-slurm:

Using the UDP Conduit with Slurm
********************************

It is also possible to configure GASNet/UDP to launch jobs with
Slurm using the following commands:

.. code-block:: bash

   export GASNET_SPAWNFN=C
   export GASNET_CSPAWN_CMD="srun -N%N %C"

Note that this configuration will not work for other conduits, as
``GASNET_SPAWNFN=C`` is specific to the UDP conduit.

See :ref:`using-slurm` for more general information about using Chapel
with Slurm and :ref:`ssh-launchers-with-slurm` for another strategy.

Troubleshooting the UDP Conduit
*******************************

I need to type a password when running my program
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configure your machines for password-less ssh. Try searching for "how to set up
passwordless ssh". You'll know you have succeeded when you can `ssh` directly to
the compute nodes without needing to type in a password each time.

I'm seeing login banners mixed with my program's output
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you are using SSH to launch jobs, you might get a
login banner printed out along with your program's output. We have
found the following setting useful to disable such printing (where
``-x`` is retained from the instructions above):

.. code-block:: bash

   export GASNET_SSH_OPTIONS="-x -o LogLevel=Error"

My console output seems to be jumbled or missing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We've had best results with console I/O and the UDP conduit when
setting:

  .. code-block:: bash

    export GASNET_ROUTE_OUTPUT=0


I'm seeing warnings from GASNet about using a higher-performance network
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

  WARNING: Using GASNet's udp-conduit, which exists for portability convenience.
  WARNING: Support was detected for native GASNet conduits: ibv
  WARNING: You should *really* use the high-performance native GASNet conduit
  WARNING: if communication performance is at all important in this program run.

Using a high-performance network, when available, is going to give much better
performance with Chapel than the UDP conduit. However, in some cases (e.g. when
comparing conduits) you might like to use the UDP conduit without these
warnings. To turn them off, use:

.. code-block:: bash

  export GASNET_QUIET=yes

I get xSocket errors when using a system with multiple IP addresses
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

   *** FATAL ERROR: Got an xSocket while spawning slave process: connect()
   failed while creating a connect socket (111:Connection refused)

Other error codes can arise, e.g. ``(60:Operation timed out)``.

You need to set ``CHPL_RT_MASTERIP`` (or ``GASNET_MASTERIP``), and possibly
``CHPL_RT_WORKERIP`` (or ``GASNET_WORKERIP``).  Please refer to:

  * :ref:`chpl-rt-masterip`
  * :ref:`chpl-rt-workerip`
  * ``$CHPL_HOME/third-party/gasnet/gasnet-src/udp-conduit/README``
  * https://gasnet.lbl.gov/dist/udp-conduit/README .

For example, when simulating multiple locales by oversubscribing
the local machine, use:

.. code-block:: bash

  export CHPL_RT_MASTERIP=127.0.0.1
  export CHPL_RT_WORKERIP=127.0.0.0  # may be optional


I get ``worker failed DNSLookup on master host name`` error messages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When running in a local, oversubscribed setting, this error can often
be resolved by setting ``CHPL_RT_MASTERIP`` as described in the
previous section.


.. _using-ofi-tcp:

------------------------------------
Using the libfabric ``tcp`` provider
------------------------------------

Chapel built with the ``ofi`` communication layer can use the libfabric ``tcp``
provider for communication over Ethernet. A Chapel build with ``CHPL_COMM=ofi``
will work with the ``tcp`` provider if it is available on the system. Unlike
the GASNet UDP conduit, running Chapel in this way requires a separate launcher
to spawn a Chapel program across multiple locales and requires a little more
configuration.

* If you are using a system launcher like Slurm (e.g.
  ``CHPL_LAUNCHER=slurm-srun`` or ``CHPL_LAUNCHER=none`` with explicit Slurm
  commands), you should make sure to build Chapel with
  ``CHPL_COMM_OFI_OOB=pmi2``. It may also be necessary to set
  ``SLURM_MPI_TYPE=pmi2`` in the environment at runtime. ``pmi2`` is an
  external dependency that is typically provided by Slurm installations.
* If you plan to run Chapel over an existing MPI installation, you can use the
  ``mpirun4ofi`` launcher.

Make sure to read the documentation for :ref:`libfabric <readme-libfabric>` to
ensure your Chapel installation is configured correctly for the ``tcp``
provider.

.. note::

   By setting ``FI_PROVIDER=tcp`` in the environment, you can force Chapel to
   use the ``tcp`` provider. See :ref:`readme-libfabric-providers` for more
   information on setting this environment variable.

