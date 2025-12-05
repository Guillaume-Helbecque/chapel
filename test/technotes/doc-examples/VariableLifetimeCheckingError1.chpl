//
// THIS TEST CASE IS AUTO-INCLUDED IN THE DOCUMENTATION
//

/* START_EXAMPLE */
// returnsref.chpl
proc refTo(ref x) ref {
  return x;
}

proc returnsRefLocal() ref // note `ref` return intent
{
  var i: int;
  return refTo(i); // returns `i` by reference
                   // but `i` goes out of scope here
}
ref r = returnsRefLocal();
var val = r; // accesses invalid memory
/* STOP_EXAMPLE */
