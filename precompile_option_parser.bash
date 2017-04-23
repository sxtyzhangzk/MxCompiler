set -e
rm -rf build
mkdir -p build
cd build
tar -zxf ../lib/antlr4-runtime.tar.gz
tar -zxf ../lib/boost_mini_1_64_0.tar.gz
g++ -c ../src/option_parser.cpp -o option_parser.o -Isrc/ -Iinclude/ -std=c++14 -O2
tar -zcf option_parser.tar.gz option_parser.o