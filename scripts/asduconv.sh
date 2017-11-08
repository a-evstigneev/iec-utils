#!/bin/dash

USAGE="Usage: $0 [-c cause_of_transmission] -h [FILE]..."

transform_asdu()
{
	local cot
	
	awk -v cot=$1 '
	BEGIN {
		ASDU[1]; ASDU[13]; 
		ASDU_TYPE; ASDU_COUNT; ASDU_ADDR; OBJ_ADDR; SQ; NUMHR; DATA; COT = cot;
		
		cmd="/usr/bin/printf %a " 2; cmd | getline twohex; close(cmd)
		if (twohex == "0x8p-2")
			FLOATFUNC = 1
		else if (twohex == "0x1p+1")
			FLOATFUNC = 2
	}

	$3 in ASDU {
		if (ASDU_TYPE == 0) {
			init_elem()
		}
		else if ($3 != ASDU_TYPE) {
			print_asdu(); init_elem()
		}
		else if ( ($3 == ASDU_TYPE) && (NUMHR < NR)) {
			assemble_asdu()
		}
	}	
	
	END { print_asdu() }

	function assemble_asdu() {
		if ( (length(DATA) + length(iehex($3))) > 498) {
			print_asdu(); init_elem()
		}
		else if ($2 == OBJ_ADDR + 1) {
			if ((SQ == 0) && (ASDU_COUNT == 1)) {
				SQ = 1; ++ASDU_COUNT; OBJ_ADDR = $2; DATA = DATA iehex($3)
			}
			else if (SQ == 1) {
				++ASDU_COUNT; OBJ_ADDR = $2; DATA = DATA iehex($3)
			}
			else if ((SQ == 0) && (ASDU_COUNT > 1)) {
				print_asdu(); init_elem()  
			}
		}
		else if ((length(DATA) + length(form_obj($2, $3))) > 498) {
			print_asdu(); init_elem()
		}
		else if ($2 != OBJ_ADDR + 1) {
			if (SQ == 0) {
				++ASDU_COUNT; OBJ_ADDR = $2; DATA = DATA form_obj($2, $3)
			}
			else if (SQ == 1) {
				print_asdu(); init_elem()
			}
		}
	}
		
	function init_elem() 
	{
		ASDU_ADDR = $1; ASDU_TYPE = $3; ASDU_COUNT = 1; SQ = 0; NUMHR = NR; OBJ_ADDR = $2; DATA = form_obj($2, $3)
	}

	function print_asdu(		asdu_hex) 
	{
	   asdu_hex = int2hex(ASDU_TYPE, 1) int2hex(SQ * 128 + ASDU_COUNT, 1) int2hex(COT, 2) int2hex(ASDU_ADDR, 2) DATA 
	   printf("%s\n", asdu_hex) 
	   DATA = "" 
	}
	
	function form_obj(objaddr, asdutype) 
	{
		return int2hex(objaddr, 3) iehex(asdutype)
	}
	
	function iehex(type,		strhex)
	{
		if (type == 1)
			strhex = int2hex($4 + qds($5), 1)
		else if (type == 13) 
			strhex = float2hex($4) int2hex(qds($5), 1)
		return strhex
	}

	function int2hex(val, nbyte,		i, valhex, strhex)
	{
		valhex = sprintf("%0*X", 2*nbyte, val)
		strhex = ""
		for (i = 0; i < nbyte; i++)
			strhex = substr(valhex, 2*i+1, 2) strhex
		return strhex
	}

	function indexchr(chr,		convert)
	{ 
		convert = "0123456789abcdef" 
		return index(convert, chr) - 1  
	}
   	
	function float2hex(val,		i, j, cmd, tempstr, signbit, exponent, significand, signstr, sum)
	{
		cmd="/usr/bin/printf %a " val
		cmd | getline tempstr
		close(cmd)	
		
		if (substr(tempstr, 1, 1) == "-")
			signbit = 2^31
		else
			signbit = 0
		
		if (FLOATFUNC == 1) {
			exponent = (substr(tempstr, index(tempstr, "p") + 1) + 3 + 127) * 2^23   
			signstr = substr(tempstr, index(tempstr, "x") + 1)
			
			j = 5; sum = 0
			for (i = 1; (i < (length(signstr) + 1)) && (j >= 0); ++i) {
				if (substr(signstr, i, 1) == ",")
					continue
				if (substr(signstr, i, 1) == "p")
					break
				sum = sum + indexchr(substr(signstr, i, 1)) * 16^j
				--j
			}
			significand = sum - 2^23        
			return int2hex(signbit + exponent + significand, 4)     
		}
		else if (FLOATFUNC == 2) {
			#exponent = (substr(tempstr, index(tempstr, "p") + 1) + 127) * 2^23 
			#signstr = substr(str, index(tempstr, ".") + 1, 6)
			
			#j = 5; sum = 0
			#for (i = 1; (i < (length(signstr))) && (j >= 0); ++i) {
			#	chr = substr(signstr, length(signstr) - i, 1)
			#	sum = sum + indexchr(substr(signstr, i, 1)) * 16^j
			#	--j
			#}
			
			#significand = sum / 2
			#num = signbit + exponent + significand 
			#return int2hex(signbit + exponent + significand, 4)     
		}
	}

	function qds(strbits,		sum, flag, i)
	{
		split(strbits, flag, ",")
		for (i in flag)
			if (flag[i] == "IV")
				sum = sum + 128
			else if (flag[i] == "NT")
				sum = sum + 64
			else if (flag[i] == "SB")
				sum = sum + 32
			else if (flag[i] == "BL")
				sum = sum + 16
			else if (flag[i] == "OV")
				sum = sum + 1
		return sum 
	}	
	'
}

cause=0

while getopts "c:h" opt; do
	case $opt in
	c) cause=$OPTARG ;;
	h) echo $USAGE; exit 0 ;;
	*) echo "Try $0 -h for more information."; exit 0 ;;
	esac
done

shift $(($OPTIND - 1))

cat $* | sort -n -k 1 -k 3 -k 2 | transform_asdu $cause
