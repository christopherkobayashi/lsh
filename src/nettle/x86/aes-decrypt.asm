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

	.file "aes-decrypt.asm"

	C aes_decrypt(struct aes_context *ctx, 
	C             unsigned length, uint8_t *dst,
	C 	      uint8_t *src)
	.text
	.align 16
	.globl aes_decrypt
	.type	aes_decrypt,@function
aes_decrypt:
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
	jz	.Ldecrypt_end
	
.Ldecrypt_block_loop:
	movl	20(%esp),%esi	C  address of context struct ctx
	movl	32(%esp),%ebp	C  address of plaintext
	AES_LOAD(%esi, %ebp)
	addl	$16, 32(%esp)	C Increment src pointer

	C  get number of rounds to do from struct	
	movl	AES_NROUNDS (%esi),%ebp	

	subl	$1,%ebp		C  one round is complete
	addl	$16,%esi	C  point to next key
.Ldecrypt_loop:
	pushl	%esi		C  save this first: we'll clobber it later

	C Why???
	xchgl	%ebx,%edx

	C First column
	AES_ROUND(_aes_decrypt_table,a,b,c,d)
C 	C a b c d
C 	movl	%eax,%esi	C  copy first in
C 	andl	$0x000000ff,%esi C  clear all but offset
C 	shll	$2,%esi		C  index in itbl1
C 	movl	AES_TABLE0 + _aes_decrypt_table (%esi),%edi
C 	movl	%ebx,%esi	C  second one
C 	shrl	$6,%esi
C 	andl	$0x000003fc,%esi C  clear all but offset bytes
C 	xorl	AES_TABLE1 + _aes_decrypt_table (%esi),%edi
C 	movl	%ecx,%esi	C  third one
C 	shrl	$14,%esi
C 	andl	$0x000003fc,%esi
C 	xorl	AES_TABLE2 + _aes_decrypt_table (%esi),%edi
C 	movl	%edx,%esi	C  fourth one
C 	shrl	$22,%esi
C 	andl	$0x000003fc,%esi
C 	xorl	AES_TABLE3 + _aes_decrypt_table (%esi),%edi
	pushl	%edi		C  save first on stack

	C // Second column
	C d a b c
	movl	%edx,%esi	C  copy first in
	andl	$0x000000ff,%esi C  clear all but offset
	shll	$2,%esi		C  index in itbl1
	movl	AES_TABLE0 + _aes_decrypt_table (%esi),%edi
	movl	%eax,%esi	C  second one
	shrl	$6,%esi
	andl	$0x000003fc,%esi C  clear all but offset bytes
	xorl	AES_TABLE1 + _aes_decrypt_table (%esi),%edi
	movl	%ebx,%esi	C  third one
	shrl	$14,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE2 + _aes_decrypt_table (%esi),%edi
	movl	%ecx,%esi	C  fourth one
	shrl	$22,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE3 + _aes_decrypt_table (%esi),%edi
	pushl	%edi

	C // Third column
	C c d a b
	movl	%ecx,%esi	C  copy first in
	andl	$0x000000ff,%esi C  clear all but offset
	shll	$2,%esi		C  index in itbl1
	movl	AES_TABLE0 + _aes_decrypt_table (%esi),%edi
	movl	%edx,%esi	C  second one
	shrl	$6,%esi
	andl	$0x000003fc,%esi C  clear all but offset bytes
	xorl	AES_TABLE1 + _aes_decrypt_table (%esi),%edi
	movl	%eax,%esi	C  third one
	shrl	$14,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE2 + _aes_decrypt_table (%esi),%edi
	movl	%ebx,%esi	C  fourth one
	shrl	$22,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE3 + _aes_decrypt_table (%esi),%edi
	pushl	%edi		C  save first on stack

	C // Fourth column
	C b c d a
	movl	%ebx,%esi	C  copy first in
	andl	$0x000000ff,%esi C  clear all but offset
	shll	$2,%esi		C  index in itbl1
	movl	AES_TABLE0 + _aes_decrypt_table (%esi),%edi
	movl	%ecx,%esi	C  second one
	shrl	$6,%esi
	andl	$0x000003fc,%esi C  clear all but offset bytes
	xorl	AES_TABLE1 + _aes_decrypt_table (%esi),%edi
	movl	%edx,%esi	C  third one
	shrl	$14,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE2 + _aes_decrypt_table (%esi),%edi
	movl	%eax,%esi	C  fourth one
	shrl	$22,%esi
	andl	$0x000003fc,%esi
	xorl	AES_TABLE3 + _aes_decrypt_table (%esi),%edi

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
	jnz	.Ldecrypt_loop

	C Foo?
	xchgl	%ebx,%edx

	C // last round
	C // first column
	C a b c d
	movl	%eax,%edi
	andl	$0x000000ff,%edi
	movl	%ebx,%ebp
	andl	$0x0000ff00,%ebp
	orl	%ebp,%edi
	movl	%ecx,%ebp
	andl	$0x00ff0000,%ebp
	orl	%ebp,%edi
	movl	%edx,%ebp
	andl	$0xff000000,%ebp
	orl	%ebp,%edi
	pushl	%edi

	C // second column
	C b c d a
	movl	%eax,%edi
	andl	$0xff000000,%edi
	movl	%ebx,%ebp
	andl	$0x000000ff,%ebp
	orl	%ebp,%edi
	movl	%ecx,%ebp
	andl	$0x0000ff00,%ebp
	orl	%ebp,%edi
	movl	%edx,%ebp
	andl	$0x00ff0000,%ebp
	orl	%ebp,%edi
	pushl	%edi

	C // third column
	C c d a b
	movl	%eax,%edi
	andl	$0x00ff0000,%edi
	movl	%ebx,%ebp
	andl	$0xff000000,%ebp
	orl	%ebp,%edi
	movl	%ecx,%ebp
	andl	$0x000000ff,%ebp
	orl	%ebp,%edi
	movl	%edx,%ebp
	andl	$0x0000ff00,%ebp
	orl	%ebp,%edi
	pushl	%edi

	C // fourth column
	C d a b c
	movl	%eax,%edi
	andl	$0x0000ff00,%edi
	movl	%ebx,%ebp
	andl	$0x00ff0000,%ebp
	orl	%ebp,%edi
	movl	%ecx,%ebp
	andl	$0xff000000,%ebp
	orl	%ebp,%edi
	movl	%edx,%ebp
	andl	$0x000000ff,%ebp
	orl	%ebp,%edi
	movl	%edi,%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
	xchgl	%ebx,%edx

	C // inverse S-box substitution
	mov	$4,%edi
.Lisubst:
	movl	%eax,%ebp
	andl	$0x000000ff,%ebp
	movb	AES_SBOX + _aes_decrypt_table (%ebp),%al
	roll	$8,%eax

	movl	%ebx,%ebp
	andl	$0x000000ff,%ebp
	movb	AES_SBOX + _aes_decrypt_table (%ebp),%bl
	roll	$8,%ebx

	movl	%ecx,%ebp
	andl	$0x000000ff,%ebp
	movb	AES_SBOX + _aes_decrypt_table (%ebp),%cl
	roll	$8,%ecx

	movl	%edx,%ebp
	andl	$0x000000ff,%ebp
	movb	AES_SBOX + _aes_decrypt_table (%ebp),%dl
	roll	$8,%edx

	decl	%edi
	jnz	.Lisubst

	xorl	(%esi),%eax	C  add last key to plaintext
	xorl	4(%esi),%ebx
	xorl	8(%esi),%ecx
	xorl	12(%esi),%edx

	C // store decrypted data back to caller's buffer
	movl	28(%esp),%edi
	movl	%eax,(%edi)
	movl	%ebx,4(%edi)
	movl	%ecx,8(%edi)
	movl	%edx,12(%edi)
	
	addl	$16, 28(%esp)	C Increment destination pointer
	subl	$16, 24(%esp)
	jnz	.Ldecrypt_block_loop

.Ldecrypt_end: 
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
.eord:
	.size	aes_decrypt,.eord-aes_decrypt
