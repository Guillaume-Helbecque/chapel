//
// THESE TEST CASES ARE AUTO-INCLUDED IN THE DOCUMENTATION
//

/* START_EXAMPLE_0 */
module DemonstrateScopes {
  proc function() {
    var f: int;
    // scope of `f` includes the body of this function
    {
      // and any nested blocks (including loops, conditionals, etc).

      var x: int;
      // `x` scope ends here, but `f` scope does not
    }
    // it would be an error to access `x` here
  }
  // it would be an error to access `f` here

  {
    var b: int;
    // `b`s scope extends to the end of this block
  }
  // it would be an error to try to use `b` here

  var g: int;
  // `g` is a global variable and its scope extends
  // to any code using this module.
}
/* STOP_EXAMPLE_0 */

/* START_EXAMPLE_1 */
module DemonstrateLifetimes {
  proc function() {
    var i: int;
    // `i`s lifetime extends to the end of this block
    ref r = i;
    // `r` refers to `i`, so it's lifetime == `i`s lifetime
    var own = new owned SomeClass();
    // `own`s lifetime extends to the end of this block,
    // (at which point the class instance may be deleted)
    var borrow = own.borrow();
    // `borrow`s lifetime extends to the end of this block
    // because its lifetime matches `own`
  }

  var global: owned SomeClass;
  proc settingGlobal() {
    var x = new owned SomeClass();
    // lifetime of x extends for entire function body

    global = x; // transfers the instance from x to global
    // leaving x storing `nil`

    // lifetime of `x` extends to here, but an attempt
    // to use `x` would result in an error from
    // compile-time nil checking.
  }
}
/* STOP_EXAMPLE_1 */

/* START_EXAMPLE_2 */
class C { var x: int; }
var globalOwned = new owned C(1);
var globalBorrow = globalOwned.borrow();

// Default lifetime inference assumes that the
// returned lifetime is the lifetime of arg,
// but that's not appropriate here.
//
// The lifetime annotation indicates that the returned value
// has the lifetime of globalBorrow.
proc returnsGlobalBorrow(arg: borrowed C)
  lifetime return globalBorrow
{
  return globalBorrow;
}
/* STOP_EXAMPLE_2 */

/* START_EXAMPLE_3 */
record Collection {
  type elementType;
  var element: elementType;
}

// Without lifetime annotation, the compiler will raise an error,
// because `this` is assumed to have larger lifetime than `arg`,
// and so the assignment will set something with a longer lifetime
// to something with a shorter lifetime.
//
// The lifetime clause `lifetime this < arg` avoids that error
// by informing the compiler that `this` (and by extension, `this.element`)
// need to have lifetime no longer than `arg`.
proc Collection.addElement(arg: elementType)
  lifetime this < arg
{
  this.element = arg;
}
/* STOP_EXAMPLE_3 */

class MyClass { }

/* START_EXAMPLE_4 */
proc myswap(ref lhs: borrowed MyClass, ref rhs: borrowed MyClass)
  lifetime lhs=rhs, rhs=lhs
{
  var tmp = lhs;
  lhs = rhs;
  rhs = tmp;
}
/* STOP_EXAMPLE_4 */
