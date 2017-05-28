set -e
cat /proc/cpuinfo | grep "model name"
cat /proc/meminfo | grep "MemTotal"
rm -rf build
mkdir -p build
cd build
echo extracting antlr generated files...
time tar -zxf ../lib/antlr_generated.tar.gz
echo extracting option parser...
time tar -zxf ../lib/option_parser.tar.gz
echo extracting antlr4...
time tar -zxf ../lib/antlr4-runtime.tar.gz
cp ../lib/libboost_program_options.a .
# echo extracting boost...
# time tar -zxf ../lib/boost_mini_1_64_0.tar.gz
cp ../lib/Makefile .
echo compiling...
time make $@
