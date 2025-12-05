//
// THIS TEST CASE IS AUTO-INCLUDED IN THE DOCUMENTATION
//

/* START_EXAMPLE */
// returnsborrow.chpl
class SomeClass { var field: int; }
proc borrowLocal() {
  var obj = new owned SomeClass();
  return obj.borrow(); // returns borrow of `obj`
  // but `obj` goes out of scope (and `delete`s the instance) here
}

var b = borrowLocal();
var y = b.field; // accesses deleted memory
/* STOP_EXAMPLE */
