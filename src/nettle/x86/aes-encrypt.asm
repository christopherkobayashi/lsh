C nettle, low-level cryptographics library
C 
C Copyright (C) 2001, 2002 Rafael R. Sevilla, Niels M�ller
C  
C The nettle library is free software; you can redistribute it and/or modify
C it under the terms of the GNU Lesser General Public License as published by
C the Free Software Foundation; either version 2.1 of the License, or (at your
C option) any later version.
C 
C The nettle library is distributed in the hope that it will be useful, but
C WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
C or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
C License for more details.
C 
C You should have received a copy of the GNU Lesser General Public License
C along with the nettle library; see the file COPYING.LIB.  If not, write to
C the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
C MA 02111-1307, USA.

C Register usage:
C
C The aes state is kept in %eax, %ebx, %ecx and %edx
C
C %esi is used as temporary, to point to the input, and to the
C subkeys, etc.
C
C %ebp is used as the round counter, and as a temporary in the final round.
C
C %edi is a temporary, often used as an accumulator.

	.file "aes-encrypt.asm"
	
	C aes_encrypt(struct aes_context *ctx, 
	C             unsigned length, uint8_t *dst,
	C 	      uint8_t *src)
	.text
	.align 16
	.globl nettle_aes_encrypt
	.type  nettle_aes_encrypt,@function
nettle_aes_encrypt:
	C save all registers that need to be saved
	pushl	%ebx		C  16(%esp)
	pushl	%ebp		C  12(%esp)
	pushl	%esi		C  8(%esp)
	pushl	%edi		C  4(%esp)

	C ctx = 20(%esp)
	C length = 24(%esp)
	C dst = 28(%esp)
	C src = 32(%esp)

	movl	24(%esp), %ebp
	testl	%ebp,%ebp
	jz	.Lend
	
.Lblock_loop:
	movl	20(%esp),%esi	C  address of context struct ctx
	movl	32(%esp),%ebp	C  address of plaintext
	AES_LOAD(%esi, %ebp)
	addl	$16, 32(%esp)	C Increment src pointer

	C  get number of rounds to do from ctx struct	
	movl	AES_NROUNDS (%esi),%ebp

	subl	$1,%ebp
	addl	$16,%esi	C  point to next key
.Lround_loop:
	pushl	%esi		C  save this first: we'll clobber it later

	AES_ROUND(_aes_encrypt_table,a,b,c,d)
	pushl	%edi		C  save first on stack

	AES_ROUND(_aes_encrypt_table,b,c,d,a)
	pushl	%edi		C  save first on stack

	AES_ROUND(_aes_encrypt_table,c,d,a,b)
	pushl	%edi		C  save first on stack

	AES_ROUND(_aes_encrypt_table,d,a,b,c)
	
	movl	%edi,%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	
	popl	%esi

	xorl	(%esi),%eax	C  add current session key to plaintext
	xorl	4(%esi),%ebx
	xorl	8(%esi),%ecx
	xorl	12(%esi),%edx
	addl	$16,%esi	C  point to next key
	decl	%ebp
	jnz	.Lround_loop

	C last round

	AES_FINAL_ROUND(a,b,c,d)
	pushl	%edi

	AES_FINAL_ROUND(b,c,d,a)
	pushl	%edi

	AES_FINAL_ROUND(c,d,a,b)
	pushl	%edi

	AES_FINAL_ROUND(d,a,b,c)

	movl	%edi,%edx
	popl	%ecx
	popl	%ebx
	popl	%eax

	C S-box substitution
	mov	$4,%edi
.Lsubst:
	AES_SUBST_BYTE(_aes_encrypt_table)

	decl	%edi
	jnz	.Lsubst

	C Add last subkey, and store encrypted data
	movl	28(%esp),%edi
	AES_STORE(%esi, %edi)
	
	addl	$16, 28(%esp)	C Increment destination pointer
	subl	$16, 24(%esp)	C Length

	C NOTE: Will loop forever if input data is not an
	C integer number of blocks.
	jnz	.Lblock_loop

.Lend: 
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
.Leord:
	.size	aes_encrypt,.Leord-aes_encrypt
