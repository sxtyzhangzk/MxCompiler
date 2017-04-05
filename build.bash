set -e
rm -rf build
mkdir -p build/generated
cd rules
java -jar ../lib/antlr-4.7-complete.jar -Dlanguage=Cpp MxLexer.g4 MxParser.g4 -o ../build/generated/ -no-listener -visitor
cd ../build
tar -zxvf ../lib/antlr4-runtime.tar.gz
g++ -c ../src/*.cpp generated/*.cpp -Isrc/ -Igenerated/ -std=c++14 -O2
g++ *.o *.a -o mxcompiler
