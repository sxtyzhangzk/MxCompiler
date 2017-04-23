set -e
rm -rf build
mkdir -p build/generated
cd rules
java -jar ../lib/antlr-4.7-complete.jar -Dlanguage=Cpp MxLexer.g4 MxParser.g4 -o ../build/generated/ -no-listener -visitor
cd ../build
tar -zxf ../lib/antlr4-runtime.tar.gz
g++ -c generated/*.cpp -Igenerated/ -Isrc/ -std=c++14 -O2 
ar -r antlr_generated.a *.o
rm generated/*.cpp
tar -zcf antlr_generated.tar.gz generated/ antlr_generated.a 