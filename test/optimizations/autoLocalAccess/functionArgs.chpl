use common;

var D = createDom({1..10});

var A: [D] int;
var B: [D] int;

// all the patterns in this test must be recognized and optimized statically

proc localQueriedDomain(ref a: [?d] int, b: [d] int){
  forall i in a.domain with (ref a) {
    a[i] +=
      b[i];
  }
  writeln(a);

  forall i in b.domain with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);

  forall i in d with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);
}

proc globalDotDomain(ref a: [A.domain] int, b: [A.domain] int){
  forall i in A.domain with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);

  forall i in B.domain with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);

  forall i in D with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);
}

proc globalDomain(ref a: [D] int, b: [D] int){
  forall i in A.domain with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);

  forall i in B.domain with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);

  forall i in D with (ref a) {
    a[i] += 
      b[i];
  }
  writeln(a);
}

proc main() {
  B = 10;

  localQueriedDomain(A, B);

  globalDotDomain(A, B);

  globalDomain(A, B);
}
