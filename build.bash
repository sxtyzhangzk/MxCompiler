set -e
rm -rf build
mkdir -p build/generated
cd rules
echo generating parser...
time java -jar ../lib/antlr-4.7-complete.jar -Dlanguage=Cpp MxLexer.g4 MxParser.g4 -o ../build/generated/ -no-listener -visitor
cd ../build
echo extracting antlr4...
time tar -zxf ../lib/antlr4-runtime.tar.gz
echo extracting boost...
time tar -zxf ../lib/boost_mini_1_64_0.tar.gz
cp ../lib/Makefile .
echo compiling...
time make
