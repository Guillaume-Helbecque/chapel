use IO;
use maps;

var messageObj = new mapTest();
var file = open("out", ioMode.r);
var readingChannel = file.reader(locking=false);

messageObj.deserialize(readingChannel);

writeln(messageObj.mapfield1[1:int(32)].bo == true);

writeln(messageObj.mapfield2[1:int(32)] == "chapel");
writeln(messageObj.mapfield2[2:int(32)] == "protobuf");
