use CommDiagnostics, CTypes;

const x = (2, 3);

startVerboseComm();

extern proc printf(fmt: c_ptrConst(c_char), x...);

on Locales(1) {
  printf("%s\n", (x(0):string + x(1):string).c_str());
}

printf("%s\n", (x(0):string + x(1):string).c_str());

stopVerboseComm();
