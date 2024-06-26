/***********************************************************************
 * Chapel implementation of "99 bottles of beer"
 *
 * by Brad Chamberlain and Steve Deitz
 * 07/13/2006 in Knoxville airport while waiting for flight home from
 *            HPLS workshop
 * compiles and runs with chpl compiler version 0.3.3211
 * for more information, contact: chapel_info@cray.com
 * modified 08/02/2006 to fix a bug workaround in the original version
 * modified 08/16/2006 to update syntax (fun -> def)
 * 
 *
 * Notes: 
 * o as in all good parallel computations, boundary conditions
 *   constitute the vast bulk of complexity in this code (invite Brad to
 *   tell you about his zany boundary condition simplification scheme)
 * o uses type inference for variables, arguments
 * o relies on integer->string coercions
 * o uses named argument passing (for documentation purposes only)
 ***********************************************************************/

// allow executable command-line specification of number of bottles 
// (e.g., ./beer -snumBottles=999999)
config const numBottles = 99;
const numVerses = numBottles+1;

// a domain to describe the space of lyrics
var LyricsSpace: domain(1) = {1..numVerses};

// array of lyrics
var Lyrics: [LyricsSpace] string;

// parallel computation of lyrics array
[verse in LyricsSpace with (ref Lyrics)] Lyrics(verse) = computeLyric(verse);

// as in any good parallel language, I/O to stdout is serialized.
// (Note that I/O to a file could be parallelized using a parallel
// prefix computation on the verse strings' lengths with file seeking)
writeln(Lyrics);


// HELPER FUNCTIONS:

proc computeLyric(verseNum) {
  var bottleNum = numBottles - (verseNum - 1);
  var nextBottle = (bottleNum + numVerses - 1)%numVerses;
  return "\n" // disguise space used to separate elements in array I/O
       + describeBottles(bottleNum, startOfVerse=true) + " on the wall, "
       + describeBottles(bottleNum) + ".\n"
       + computeAction(bottleNum)
       + describeBottles(nextBottle) + " on the wall.\n";
}


proc describeBottles(bottleNum, startOfVerse = false) {
  var bottleDescription:string;
  if bottleNum then bottleDescription = bottleNum:string;
  else {
    var first = (if startOfVerse then "N" 
                                 else "n") + "o more";
    bottleDescription = first;
  }
  return bottleDescription 
       + " bottle" + (if (bottleNum == 1) then "" else "s") 
       + " of beer";
}


proc computeAction(bottleNum) {
  return if (bottleNum == 0) then "Go to the store and buy some more, "
                             else "Take one down and pass it around, ";
}
