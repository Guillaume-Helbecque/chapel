.. _readme-lifetime-checking:

===========================
Checking Variable Lifetimes
===========================

As of Chapel 1.18, Chapel includes a compiler component called the
lifetime checker. The lifetime checker produces errors at compile time to
reveal potential memory errors. Note that the Chapel lifetime checker is
not complete - that is, there are programs with memory errors that it
will not detect. However, we hope that it offers a good balance between
being easy to work with and catching common memory errors at
compile-time. See also :ref:`readme-nil-checking` which discusses a
related component that discovers nil dereferences at compile time.

Defining Scope and Lifetime
===========================

Scope
+++++

Variables in Chapel have a lexical scope within which it is legal to
access the variable. For example:

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeChecking.chpl
 :language: chapel
 :start-after: START_EXAMPLE_0
 :end-before: STOP_EXAMPLE_0

A scope can be contained in another scope. For example, the scope of ``x``
is contained within the scope of ``f`` in the above example. In other
words, anywhere that ``x`` can be accessed, ``f`` can also be accessed.
In such a case we say that the scope of ``x`` is smaller than the scope of
``f``.

Lifetime
++++++++

The lifetime of a variable indicates when that variable can be safely
used.

 * Variables that cannot refer to another value, such as numeric
   variables, have a lifetime equal to their lexical scope.
 * Variables that can refer to other variables include ``borrowed`` class
   instances and ``ref`` variables. These variables get their lifetime
   from the lifetime of the variable that they refer to.
 * ``owned`` and ``shared`` variables have lifetime equal to their scope.

Note that ``owned`` and ``shared`` variables can be returned or assigned
without impacting their lifetime. The lifetime checker just checks that a
``borrow`` from such a variable does not outlive the variable itself.

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeChecking.chpl
 :language: chapel
 :start-after: START_EXAMPLE_1
 :end-before: STOP_EXAMPLE_1

Similarly to scopes, lifetimes may be contained within each other.
Ultimately, a lifetime is just the scope of some variable, and so we can
say that one lifetime is smaller or larger than another, just as we can
say that a scope is smaller or larger than another scope.

Example Errors
==============

The lifetime checker is designed to catch errors such as:

 * returning a reference to or borrow from a function-local variable
 * assigning a value with a shorter lifetime to something with a larger scope

When the lifetime for a variable is smaller than its scope, that usually
means that there is some point in the program where accessing that
variable could lead to a memory error. There are some cases where the
analysis indicates a memory error could occur, but a human programmer
might know that it cannot for other reasons.

Returning a Reference to a Local Variable
+++++++++++++++++++++++++++++++++++++++++

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError1.chpl
 :language: chapel
 :start-after: START_EXAMPLE
 :end-before: STOP_EXAMPLE

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError1.good

Returning a Borrow From a Local Owned Instance
++++++++++++++++++++++++++++++++++++++++++++++

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError2.chpl
 :language: chapel
 :start-after: START_EXAMPLE
 :end-before: STOP_EXAMPLE

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError2.good

Assigning a Borrow to something with Longer Scope
+++++++++++++++++++++++++++++++++++++++++++++++++

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError3.chpl
 :language: chapel
 :start-after: START_EXAMPLE
 :end-before: STOP_EXAMPLE

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeCheckingError3.good

Lifetime Inference
==================

The lifetime checker starts by inferring the lifetime of each variable.
It considers the ways that the variable is set:

 * if the variable is a reference to another variable, then
   its lifetime will be the scope of that variable
 * if a borrow is assigned or initialized from another variable, then
   its lifetime will be at most the lifetime of the other variable
 * if the variable is set by a function call, then the lifetime is inferred
   according to rules described below

Inference proceeds until the minimum inferred lifetime of each variable is
established.

Inferred Lifetimes of Arguments
+++++++++++++++++++++++++++++++

For methods, the ``this`` argument is assumed to have longer lifetime than the
actual arguments and only the ``this`` argument is assumed to have a lifetime
that can be returned.

For non-methods, all formals are considered to have a lifetime that can be
returned.

Inferred Lifetime of Function Call Results
++++++++++++++++++++++++++++++++++++++++++

For ``x = f(a, b, c)``, the lifetime of ``x`` is inferred to be the
minimum lifetime of the arguments ``a``, ``b``, ``c`` that have lifetimes
that could be returned.

For a method call, such as ``y = receiver.f(a, b, c)``, the lifetime will
be inferred to be the lifetime of ``receiver``.

If these inferred lifetimes are not appropriate for a function, the lifetimes
can be specified with a lifetime annotation.

Lifetime Annotations
====================

Certain functions need to override the default lifetime inference rules.
This can be accomplished by placing a ``lifetime`` clause after the
return type. These ``lifetime`` clauses share some similarities with
``where`` clauses. For example:

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeChecking.chpl
 :language: chapel
 :start-after: START_EXAMPLE_2
 :end-before: STOP_EXAMPLE_2

Other functions need to assert a relationship between the lifetimes of
their arguments. This pattern comes up with functions that append some
data to a data structure.

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeChecking.chpl
 :language: chapel
 :start-after: START_EXAMPLE_3
 :end-before: STOP_EXAMPLE_3

Note that the lifetime clause needs to be written in terms of formal
arguments, including ``this`` for methods, and possible outer variables.
In particular, in the above, the constraint is between ``this`` and
``arg`` rather than ``this.element`` and ``arg``. ``this.element`` will
have its lifetime inferred to be the lifetime of ``this``, so these are
equivalent.

In some cases, it is more natural to write the lifetime annotation in
terms of what assignments the function may make. For example:

.. literalinclude:: ../../../test/technotes/doc-examples/VariableLifetimeChecking.chpl
 :language: chapel
 :start-after: START_EXAMPLE_4
 :end-before: STOP_EXAMPLE_4

Here the lifetime checker ensures that the lifetimes of the actual
arguments are suitable for performing the assignments between formals
that are indicated in the lifetime clause ``lifetime lhs=rhs, rhs=lhs``.
