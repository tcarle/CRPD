int main(){
  //4 instructions per block
  asm(".align \nstart: add r0, r0, #1\nadd r0,r0,#1\ncmp r0,r1\nbeq bb2\n add r2,r2,#1\n add r2,r2,#1\n add r2,r2,#1\n add r2,r2,#1\n add r3,r3,#1\n add r3,r3,#1\n add r3,r3,#1\n add r3,r3,#1\n add r4,r4,#1\n add r4,r4,#1\n add r4,r4,#1\n b join\n toto: add r5,r5,#1\n add r5,r5,#1\n add r5,r5,#1\n add r5,r5,#1\n add r6,r6,#1\n add r6,r6,#1\n add r6,r6,#1\n add r6,r6,#1\n join: add r4,r4,#1\n add r4,r4,#1\n add r4,r4,#1\n b start");
  return 0;
}
