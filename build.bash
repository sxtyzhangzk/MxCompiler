set -e
rm -rf build
mkdir -p build/generated
cd rules
java -jar ../lib/antlr-4.7-complete.jar -Dlanguage=Cpp MxLexer.g4 MxParser.g4 -o ../build/generated/ -no-listener -visitor
cd ../build
tar -zxf ../lib/antlr4-runtime.tar.gz
tar -zxf ../lib/boost_mini_1_64_0.tar.gz
cp ../lib/Makefile .
make
