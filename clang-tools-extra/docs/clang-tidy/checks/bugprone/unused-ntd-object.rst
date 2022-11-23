.. title:: clang-tidy - bugprone-unused-ntd-object

bugprone-unused-ntd-object
==========================

Finds unused variables with nontrivial destructors that are unlikely to be used
as RAII.

Options
-------

.. option:: CheckedTypes

   Semicolon-separated list of types to check. The variable is checked if
   the its type is from ``CheckedTypes``.
   By default the following types are checked:
   ``absl::Status``

By default unused variables are reported only if they have trivial destructors,
otherwise the destructor call is a usage of the variable, a pattern used in RAII.

Some objects however have destructors because of internal state management, not
to enable RAII. One such examle is ``absl::Status``. This class has reference counting (and thus a non-trivial destructor), but it is a "meaningless" usage. Consider the following code.

.. code-block:: c++

  {
    absl::Status status = call_some_function();
  } // status.~Status() is called here

This does not cause unused variable warning, but is likely to contain a bug.

This check is not absolutely precise and aims to capture the most common scenarios like the one above.