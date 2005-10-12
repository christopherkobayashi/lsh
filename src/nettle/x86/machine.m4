dnl AES_LOAD(key, src)
dnl Loads the next block of data from src, and add the subkey pointed
dnl to by key.
dnl Note that x86 allows unaligned accesses.
dnl Would it be preferable to interleave the loads and stores?
define(<AES_LOAD>, <
	movl	($2),%eax
	movl	4($2),%ebx
	movl	8($2),%ecx
	movl	12($2),%edx
	
	xorl	($1),%eax
	xorl	4($1),%ebx
	xorl	8($1),%ecx
	xorl	12($1),%edx>)dnl

dnl AES_STORE(key, dst)
dnl Adds the subkey pointed to by %esi to %eax-%edx,
dnl and stores the result in the area pointed to by %edi.
dnl Note that x86 allows unaligned accesses.
dnl Would it be preferable to interleave the loads and stores?
define(<AES_STORE>, <
	xorl	($1),%eax
	xorl	4($1),%ebx
	xorl	8($1),%ecx
	xorl	12($1),%edx

	movl	%eax,($2)
	movl	%ebx,4($2)
	movl	%ecx,8($2)
	movl	%edx,12($2)>)dnl

dnl AES_ROUND(table,a,b,c,d)
dnl Computes one word of the AES round. Leaves result in %edi.
define(<AES_ROUND>, <
	movl	%e<>$2<>x, %esi
	andl	<$>0xff, %esi
	shll	<$>2,%esi		C  index in table
	movl	AES_TABLE0 + $1 (%esi),%edi
	movl	%e<>$3<>x, %esi
	shrl	<$>6,%esi
	andl	<$>0x000003fc,%esi C  clear all but offset bytes
	xorl	AES_TABLE1 + $1 (%esi),%edi
	movl	%e<>$4<>x,%esi	C  third one
	shrl	<$>14,%esi
	andl	<$>0x000003fc,%esi
	xorl	AES_TABLE2 + $1 (%esi),%edi
	movl	%e<>$5<>x,%esi	C  fourth one
	shrl	<$>22,%esi
	andl	<$>0x000003fc,%esi
	xorl	AES_TABLE3 + $1 (%esi),%edi>)dnl

dnl AES_FINAL_ROUND(a, b, c, d)
dnl Computes one word of the final round. Leaves result in %edi.
dnl Note that we have to quote $ in constants.
define(<AES_FINAL_ROUND>, <
	C FIXME: Perform substitution on least significant byte here,
	C to save work later.
	movl	%e<>$1<>x,%edi
	andl	<$>0x000000ff,%edi
	movl	%e<>$2<>x,%ebp
	andl	<$>0x0000ff00,%ebp
	orl	%ebp,%edi
	movl	%e<>$3<>x,%ebp
	andl	<$>0x00ff0000,%ebp
	orl	%ebp,%edi
	movl	%e<>$4<>x,%ebp
	andl	<$>0xff000000,%ebp
	orl	%ebp,%edi>)dnl

dnl AES_SUBST_BYTE(table)
dnl Substitutes the least significant byte of
dnl each of eax, ebx, ecx and edx, and also rotates
dnl the words one byte to the left.
define(<AES_SUBST_BYTE>, <
	movl	%eax,%ebp
	andl	<$>0x000000ff,%ebp
	movb	AES_SBOX + $1 (%ebp),%al
	roll	<$>8,%eax

	movl	%ebx,%ebp
	andl	<$>0x000000ff,%ebp
	movb	AES_SBOX + $1 (%ebp),%bl
	roll	<$>8,%ebx

	movl	%ecx,%ebp
	andl	<$>0x000000ff,%ebp
	movb	AES_SBOX + $1 (%ebp),%cl
	roll	<$>8,%ecx

	movl	%edx,%ebp
	andl	<$>0x000000ff,%ebp
	movb	AES_SBOX + $1 (%ebp),%dl
	roll	<$>8,%edx>)dnl

C OFFSET(i)
C Expands to 4*i, or to the empty string if i is zero
define(<OFFSET>, <ifelse($1,0,,eval(4*$1))>)
