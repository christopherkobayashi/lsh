#! /usr/bin/awk -f

# Run this filter on the output of
#
#   objdump -h libnettle.a

function hex2int (hex) {
  n = 0;
  hex = tolower(hex);
  for (i = 1; i<=length(hex); i++)
    {
      n = n * 16 + index("0123456789abcdef", substr(hex,i,1)) - 1;
    }

  return n;
}

BEGIN {
    print "file            text-size  data-size  rodata-size";
    text_total = 0;
    data_total = 0;
    rodata_total = 0;
    name = "";
    filter = ENVIRON["FILTER"];
    if (!filter)
      filter = ".*";
    else
      printf "Filter: %s\n", filter;
}

/elf32/ {
  if (name)
    {
      printf "%25s %6x   %6x   %6x\n", name, text_size, data_size, rodata_size;
      text_total += text_size;
      data_total += data_size;
      rodata_total += rodata_size; 
    } 
  if ($1 ~ filter)
    {
      name = $1; text_size = data_size = rodata_size = 0;
      sub(/^[^-]*_a-/, "", name);
    }
  else
    name = ""
}
/\.text/ { text_size = hex2int($3) }
/\.data/ { data_size = hex2int($3); }
/\.rodata/ { rodata_size = hex2int($3); }

END {
  printf "%25s %6x   %6x   %6x\n", "TOTAL", text_total, data_total, rodata_total;
}
