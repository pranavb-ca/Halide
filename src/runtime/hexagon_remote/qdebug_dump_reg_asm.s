/**=============================================================================
 Function to dump HVX register contents in unsigned 8 bit format.
 =============================================================================**/

#include "qdebug_asm.h"

	.text
   .p2align 4                         
   .globl test           
   .type       test, @function
test:
	{ r0 = add(r0,r0) }
	{ JUMPR R31 }

   .size       test, .-test
 
   .text
   .p2align 4                         
   .globl qdebug_dump_reg_asm           
   .type       qdebug_dump_reg_asm, @function
qdebug_dump_reg_asm:
   {
   ALLOCFRAME(#(8*8 + 2*8)) // r + p
   }
   QD print_u8 V_all
   {
   DEALLOC_RETURN
   }
   .size       qdebug_dump_reg_asm, .-qdebug_dump_reg_asm
