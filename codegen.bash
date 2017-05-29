cd build
cat > program.mx
./mxcompiler program.mx -o program.asm --fdisable-access-protect --optim-reg-alloc --optim-inline --optim-loop-invariant --optim-dead-code --optim-gvn
cat program.asm
