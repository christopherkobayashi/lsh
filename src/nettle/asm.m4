changequote(<,>)dnl
dnl (progn (modify-syntax-entry ?< "(>") (modify-syntax-entry ?> ")<") )

dnl FORTRAN style comment character
define(<C>, <
dnl>)dnl

dnl including files from the srcdir
define(<include_src>, <include(srcdir/$1)>)dnl

dnl Pseudo ops

define(<PROLOGUE>,
<ifelse(ELF_STYLE,yes,
<.globl C_NAME($1)
.type C_NAME($1),@function
C_NAME($1):>,
<.globl C_NAME($1)
C_NAME($1):>)>)

define(<EPILOGUE>,
<ifelse(ELF_STYLE,yes,
<.L$1end:
.size C_NAME($1), .L$1end - C_NAME($1)>,)>)


dnl Struct defining macros

dnl STRUCTURE(prefix) 
define(<STRUCTURE>, <define(<SOFFSET>, 0)define(<SPREFIX>, <$1>)>)dnl

dnl STRUCT(name, size)
define(<STRUCT>,
<define(SPREFIX<_>$1, SOFFSET)dnl
 define(<SOFFSET>, eval(SOFFSET + ($2)))>)dnl

dnl UNSIGNED(name)
define(<UNSIGNED>, <STRUCT(<$1>, 4)>)dnl

dnl Offsets in aes_ctx and aes_table
STRUCTURE(AES)
  STRUCT(KEYS, 4*60)
  UNSIGNED(NROUNDS)

define(AES_SBOX_SIZE,	256)dnl
define(AES_IDX_SIZE,	16)dnl
define(AES_TABLE_SIZE,	1024)dnl

STRUCTURE(AES)
  STRUCT(SBOX, AES_SBOX_SIZE)

  STRUCT(IDX1, AES_IDX_SIZE)
  STRUCT(IDX2, AES_IDX_SIZE)
  STRUCT(IDX3, AES_IDX_SIZE)

  STRUCT(SIDX1, AES_IDX_SIZE)
  STRUCT(SIDX3, AES_IDX_SIZE)

  STRUCT(TABLE0, AES_TABLE_SIZE)
  STRUCT(TABLE1, AES_TABLE_SIZE)
  STRUCT(TABLE2, AES_TABLE_SIZE)
  STRUCT(TABLE3, AES_TABLE_SIZE)
