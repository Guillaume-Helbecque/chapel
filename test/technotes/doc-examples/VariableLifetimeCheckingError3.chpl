//
// THIS TEST CASE IS AUTO-INCLUDED IN THE DOCUMENTATION
//

/* START_EXAMPLE */
// assignsborrow.chpl
class SomeClass { }

{
  var bor: borrowed SomeClass;
  {
    var obj = new owned SomeClass();
    bor = obj.borrow(); // borrow of `obj` escapes
    // but `obj` goes out of scope (and `delete`s the instance) here
  }
  writeln(bor); // uses freed memory
}
/* STOP_EXAMPLE */
